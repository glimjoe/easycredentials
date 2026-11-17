/*
 * Copyright (C) 2026 EasyCredentials contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config-keepassx-tests.h"
#include "core/Config.h"
#include "core/CustomData.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "crypto/Crypto.h"
#include "gui/Application.h"
#include "gui/DatabaseWidget.h"
#include "gui/MainWindow.h"
#include "gui/entry/EditEntryWidget.h"
#include "gui/remote/RemoteHandler.h"
#include "gui/remote/RemoteSettings.h"
#include "gui/remote/DatabaseSettingsWidgetRemote.h"
#include "keys/CompositeKey.h"
#include "keys/PasswordKey.h"

#include <QFile>
#include <QDir>
#include <QProgressDialog>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QTimer>
#include <QSignalSpy>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>

namespace
{
    QString command(const QStringList& args)
    {
        QStringList quoted;
        quoted << '"' + QCoreApplication::applicationFilePath() + '"' << "--remote-helper";
        for (const auto& arg : args) {
            quoted << '"' + arg + '"';
        }
        return quoted.join(' ');
    }

    QSharedPointer<CompositeKey> key(const QString& password = "a")
    {
        auto result = QSharedPointer<CompositeKey>::create();
        result->addKey(QSharedPointer<PasswordKey>::create(password));
        return result;
    }

    QByteArray contents(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return file.readAll();
    }

    RemoteParams transfer(const QString& remote)
    {
        RemoteParams params;
        params.name = "VPS";
        params.downloadCommand = command({"copy", remote, "{TEMP_DATABASE}"});
        params.uploadCommand = command({"copy", "{TEMP_DATABASE}", remote});
        return params;
    }

    int runHelper(const QStringList& args)
    {
        const auto action = args.value(2);
        if (action == "copy") {
            if (!args.value(5).isEmpty()) {
                QFile counter(args.value(5));
                if (!counter.open(QIODevice::Append) || counter.write("upload\n") != 7) {
                    return 11;
                }
            }
            QFile::remove(args.value(4));
            return QFile::copy(args.value(3), args.value(4)) ? 0 : 7;
        }
        if (action == "fail") {
            return 7;
        }
        if (action == "copy-when-online") {
            if (!QFile::exists(args.value(5))) {
                return 7;
            }
            QFile::remove(args.value(4));
            return QFile::copy(args.value(3), args.value(4)) ? 0 : 7;
        }
        if (action == "sleep") {
            QThread::msleep(5000);
            return 0;
        }
        if (action == "stdin") {
            QFile input;
            QFile output(args.value(3));
            if (!input.open(stdin, QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
                return 8;
            }
            const auto data = input.readAll();
            return output.write(data) == data.size() ? 0 : 9;
        }
        return 10;
    }
}

class TestRemoteSync : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(Crypto::init());
        Config::createTempFileInstance();
        Application::bootstrap();
        m_mainWindow.reset(new MainWindow());
    }

    void init()
    {
        config()->resetToDefaults();
        config()->set(Config::AutoSaveAfterEveryChange, false);
        config()->set(Config::AutoSaveOnExit, false);
        config()->set(Config::GUI_CheckForUpdates, false);
        config()->set(Config::UpdateCheckMessageShown, true);
        config()->set(Config::GUI_ShowExpiredEntriesOnDatabaseUnlock, false);
        m_directory.reset(new QTemporaryDir());
        QVERIFY(m_directory->isValid());
        m_local = m_directory->filePath("local database.kdbx");
        m_remote = m_directory->filePath("remote database.kdbx");
        const auto fixture = QString(KEEPASSX_TEST_DATA_DIR) + "/NewDatabase.kdbx";
        QVERIFY(QFile::copy(fixture, m_local));
        QVERIFY(QFile::copy(fixture, m_remote));
        m_db = QSharedPointer<Database>::create();
        QVERIFY(m_db->open(m_local, key()));
    }

    void testSettingsSurviveDatabaseSave()
    {
        const auto params = transfer(m_remote);
        RemoteSettings settings(m_db);
        settings.addRemoteParams(new RemoteParams(params));
        settings.saveSettings();
        QVERIFY(m_db->save());
        auto reopened = QSharedPointer<Database>::create();
        QVERIFY(reopened->open(m_local, key()));
        RemoteSettings restored(reopened);
        const auto* actual = restored.getRemoteParams("VPS");
        QVERIFY(actual);
        QCOMPARE(actual->downloadCommand, params.downloadCommand);
        QCOMPARE(actual->uploadCommand, params.uploadCommand);
        QVERIFY(reopened->metadata()->customData()->isProtected(CustomData::RemoteProgramSettings));
        restored.removeRemoteParams("VPS");
        restored.saveSettings();
        RemoteSettings removed(reopened);
        QVERIFY(removed.getAllRemoteParams().isEmpty());
    }

    void testDownloadAndTemporaryCleanup()
    {
        RemoteHandler handler;
        QString path;
        {
            const auto params = transfer(m_remote);
            const auto result = handler.download(&params);
            QVERIFY2(result.success, qPrintable(result.errorMessage));
            path = result.filePath;
            QCOMPARE(contents(path), contents(m_remote));
        }
        QVERIFY(!QFile::exists(path));
    }

    void testAutomaticApprovalIsLocalAndInvalidatedByCommandChanges()
    {
        const auto params = transfer(m_remote);
        enableAutomatic(params);
        RemoteSettings restored(m_db);
        QVERIFY(restored.automaticSyncParams());
        QCOMPARE(restored.automaticSyncIntervalMinutes(), 1);
        QVERIFY(!m_db->metadata()->customData()->value(CustomData::RemoteProgramSettings).contains("commandHash"));

        const auto otherPath = m_directory->filePath("another device.kdbx");
        QVERIFY(QFile::copy(m_local, otherPath));
        auto other = QSharedPointer<Database>::create();
        QVERIFY(other->open(otherPath, key()));
        QVERIFY(!RemoteSettings(other).automaticSyncParams());

        auto changed = params;
        changed.downloadInput = "unreviewed command input";
        restored.addRemoteParams(new RemoteParams(changed));
        restored.saveSettings();
        QVERIFY(!RemoteSettings(m_db).automaticSyncParams());
    }

    void testAutomaticSyncBeforeEditingUsesRemoteEntry()
    {
        const auto entries = m_db->rootGroup()->entriesRecursive();
        QVERIFY(!entries.isEmpty());
        const auto id = entries.first()->uuid();
        enableAutomatic(transfer(m_remote));
        auto remote = QSharedPointer<Database>::create();
        QVERIFY(remote->open(m_remote, key()));
        remote->rootGroup()->findEntryByUuid(id)->setTitle("Updated on another device");
        QVERIFY(remote->save());

        DatabaseWidget widget(m_db);
        auto* previous = m_db->rootGroup()->findEntryByUuid(id);
        QVERIFY(QMetaObject::invokeMethod(&widget, "switchToEntryEdit", Qt::DirectConnection,
                                         Q_ARG(Entry*, previous)));
        QCOMPARE(widget.currentMode(), DatabaseWidget::Mode::EditEntryMode);
        auto* editor = widget.findChild<EditEntryWidget*>("editEntryWidget");
        QVERIFY(editor);
        QCOMPARE(editor->currentEntry()->title(), QString("Updated on another device"));
    }

    void testFailedPreEditDownloadDoesNotEnterEditor()
    {
        auto params = transfer(m_remote);
        params.downloadCommand = command({"fail"});
        enableAutomatic(params);
        const auto beforeLocal = contents(m_local);
        const auto beforeRemote = contents(m_remote);
        DatabaseWidget widget(m_db);
        widget.createEntry();
        QCOMPARE(widget.currentMode(), DatabaseWidget::Mode::ViewMode);
        QCOMPARE(contents(m_local), beforeLocal);
        QCOMPARE(contents(m_remote), beforeRemote);
    }

    void testSavingAutomaticallyMergesUploadsOnce()
    {
        auto params = transfer(m_remote);
        const auto counter = m_directory->filePath("upload count.txt");
        params.uploadCommand = command({"copy", "{TEMP_DATABASE}", m_remote, counter});
        enableAutomatic(params);
        auto remote = QSharedPointer<Database>::create();
        QVERIFY(remote->open(m_remote, key()));
        const auto remoteId = addEntry(remote, "Remote pending change")->uuid();
        QVERIFY(remote->save());

        DatabaseWidget widget(m_db);
        const auto localId = addEntry(m_db, "Local saved change")->uuid();
        QVERIFY(widget.save());
        QVERIFY(RemoteSettings(m_db).hasPendingUpload());
        QTRY_VERIFY_WITH_TIMEOUT(!RemoteSettings(m_db).hasPendingUpload(), 5000);
        auto uploaded = QSharedPointer<Database>::create();
        QVERIFY(uploaded->open(m_remote, key()));
        QVERIFY(uploaded->rootGroup()->findEntryByUuid(localId));
        QVERIFY(uploaded->rootGroup()->findEntryByUuid(remoteId));
        QVERIFY(m_db->rootGroup()->findEntryByUuid(remoteId));
        QTest::qWait(600);
        QCOMPARE(contents(counter), QByteArray("upload\n"));
    }

    void testPeriodicRefreshSkipsUnsavedAndPendingLocalChanges()
    {
        enableAutomatic(transfer(m_remote));
        DatabaseWidget widget(m_db);
        const auto localId = addEntry(m_db, "Not uploaded yet")->uuid();
        const auto beforeLocal = contents(m_local);
        const auto beforeRemote = contents(m_remote);
        QVERIFY(QMetaObject::invokeMethod(&widget, "onRemoteSyncTimeout", Qt::DirectConnection));
        QCOMPARE(contents(m_local), beforeLocal);
        QCOMPARE(contents(m_remote), beforeRemote);
        QVERIFY(m_db->rootGroup()->findEntryByUuid(localId));

        QVERIFY(widget.save());
        QVERIFY(RemoteSettings(m_db).hasPendingUpload());
        const auto savedLocal = contents(m_local);
        QVERIFY(QMetaObject::invokeMethod(&widget, "onRemoteSyncTimeout", Qt::DirectConnection));
        QCOMPARE(contents(m_local), savedLocal);
        QCOMPARE(contents(m_remote), beforeRemote);
    }

    void testPeriodicPullPreservesEncryptedHistoryAndDoesNotUpload()
    {
        enableAutomatic(transfer(m_remote));
        DatabaseWidget widget(m_db);
        QSignalSpy merged(&widget, &DatabaseWidget::databaseMerged);
        auto remote = QSharedPointer<Database>::create();
        QVERIFY(remote->open(m_remote, key()));
        const auto remoteId = addEntry(remote, "Periodic update")->uuid();
        QVERIFY(remote->save());
        const auto beforeRemote = contents(m_remote);
        QVERIFY(QMetaObject::invokeMethod(&widget, "onRemoteSyncTimeout", Qt::DirectConnection));
        QVERIFY(m_db->rootGroup()->findEntryByUuid(remoteId));
        QCOMPARE(merged.count(), 1);
        QVERIFY(!RemoteSettings(m_db).hasPendingUpload());
        QCOMPARE(contents(m_remote), beforeRemote);
        const auto history = QDir(m_directory->filePath("backups")).entryList({"*.kdbx"}, QDir::Files);
        QVERIFY(!history.isEmpty());
        auto previous = QSharedPointer<Database>::create();
        QVERIFY(previous->open(m_directory->filePath("backups/") + history.first(), key()));
        QVERIFY(!previous->rootGroup()->findEntryByUuid(remoteId));
        const auto afterRefresh = contents(m_local);
        QTest::qWait(600);
        QCOMPARE(contents(m_local), afterRefresh);
        QCOMPARE(contents(m_remote), beforeRemote);
    }

    void testUploadFailureRemainsPendingAcrossLock()
    {
        auto params = transfer(m_remote);
        params.uploadCommand = command({"fail"});
        enableAutomatic(params);
        DatabaseWidget widget(m_db);
        const auto localId = addEntry(m_db, "Offline saved change")->uuid();
        QVERIFY(widget.save());
        const auto beforeRemote = contents(m_remote);
        QVERIFY(QMetaObject::invokeMethod(&widget, "onRemoteUploadTimeout", Qt::DirectConnection));
        QVERIFY(RemoteSettings(m_db).hasPendingUpload());
        QVERIFY(widget.lock());
        QTest::qWait(400);
        QCOMPARE(contents(m_remote), beforeRemote);
        auto reopened = QSharedPointer<Database>::create();
        QVERIFY(reopened->open(m_local, key()));
        QVERIFY(reopened->rootGroup()->findEntryByUuid(localId));
        QVERIFY(RemoteSettings(reopened).hasPendingUpload());
    }

    void testHistoryFailureBlocksSave()
    {
        enableAutomatic(transfer(m_remote));
        QFile blocker(m_directory->filePath("backups"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        DatabaseWidget widget(m_db);
        const auto beforeLocal = contents(m_local);
        const auto beforeRemote = contents(m_remote);
        const auto id = addEntry(m_db, "Keep unsaved edit")->uuid();
        QVERIFY(!widget.save());
        QCOMPARE(contents(m_local), beforeLocal);
        QCOMPARE(contents(m_remote), beforeRemote);
        QVERIFY(m_db->rootGroup()->findEntryByUuid(id));
        QVERIFY(m_db->isModified());
    }

    void testOfflineUploadRetriesMergeBeforeUploading()
    {
        auto params = transfer(m_remote);
        const auto onlinePath = m_directory->filePath("connection is online");
        params.uploadCommand = command({"copy-when-online", "{TEMP_DATABASE}", m_remote, onlinePath});
        enableAutomatic(params);
        DatabaseWidget widget(m_db);
        const auto localId = addEntry(m_db, "Saved during outage")->uuid();
        QVERIFY(widget.save());
        QVERIFY(QMetaObject::invokeMethod(&widget, "onRemoteUploadTimeout", Qt::DirectConnection));
        QVERIFY(RemoteSettings(m_db).hasPendingUpload());
        auto* retry = widget.findChild<QTimer*>("remoteUploadTimer");
        QVERIFY(retry && retry->isActive());
        QCOMPARE(retry->interval(), 60000);

        auto remote = QSharedPointer<Database>::create();
        QVERIFY(remote->open(m_remote, key()));
        const auto remoteId = addEntry(remote, "Another device changed during outage")->uuid();
        QVERIFY(remote->save());
        QFile online(onlinePath);
        QVERIFY(online.open(QIODevice::WriteOnly));
        online.close();
        QVERIFY(QMetaObject::invokeMethod(&widget, "onRemoteUploadTimeout", Qt::DirectConnection));
        QVERIFY(!RemoteSettings(m_db).hasPendingUpload());
        auto uploaded = QSharedPointer<Database>::create();
        QVERIFY(uploaded->open(m_remote, key()));
        QVERIFY(uploaded->rootGroup()->findEntryByUuid(localId));
        QVERIFY(uploaded->rootGroup()->findEntryByUuid(remoteId));
    }

    void testAutomaticSettingsUiRequiresLocalOptIn()
    {
        DatabaseSettingsWidgetRemote page;
        page.loadSettings(m_db);
        auto* enabled = page.findChild<QCheckBox*>("automaticSyncCheckBox");
        auto* interval = page.findChild<QSpinBox*>("syncIntervalMinutes");
        QVERIFY(enabled && interval);
        QVERIFY(!enabled->isChecked());
        QCOMPARE(interval->value(), 5);
        page.findChild<QLineEdit*>("nameLineEdit")->setText("VPS Mac");
        page.findChild<QLineEdit*>("downloadCommand")->setText(command({"copy", m_remote, "{TEMP_DATABASE}"}));
        page.findChild<QLineEdit*>("uploadCommand")->setText(command({"copy", "{TEMP_DATABASE}", m_remote}));
        enabled->setChecked(true);
        interval->setValue(2);
        page.findChild<QPushButton*>("saveSettingsButton")->click();
        QVERIFY(page.saveSettings());
        RemoteSettings restored(m_db);
        QVERIFY(restored.automaticSyncParams());
        QCOMPARE(restored.automaticSyncParams()->name, QString("VPS Mac"));
        QCOMPARE(restored.automaticSyncIntervalMinutes(), 2);
        QVERIFY(restored.hasPendingUpload());
    }

    void testHistoryRetentionKeepsOtherDatabaseBackups()
    {
        enableAutomatic(transfer(m_remote));
        DatabaseWidget widget(m_db);
        QVERIFY(widget.save());
        QDir history(m_directory->filePath("backups"));
        const auto files = history.entryList({"*.kdbx"}, QDir::Files);
        QCOMPARE(files.size(), 1);
        const auto name = files.first();
        const int marker = name.lastIndexOf(".autosync-");
        const auto prefix = name.left(name.indexOf('-', marker + 10) + 1);
        for (int i = 0; i < 105; ++i) {
            QVERIFY(QFile::copy(m_local, history.filePath(prefix + QString::number(i) + ".kdbx")));
        }
        const auto unrelated = history.filePath("other-database-backup.kdbx");
        QVERIFY(QFile::copy(m_local, unrelated));
        QVERIFY(widget.save());
        QVERIFY(QFile::exists(unrelated));
        QCOMPARE(history.entryList({prefix + "*.kdbx"}, QDir::Files).size(), 100);
    }

    void testStandardInputAndEndOfFile()
    {
        auto params = transfer(m_remote);
        params.downloadCommand = command({"stdin", "{TEMP_DATABASE}"});
        params.downloadInput = "input with spaces";
        RemoteHandler handler;
        auto result = handler.download(&params);
        QVERIFY2(result.success, qPrintable(result.errorMessage));
        QCOMPARE(contents(result.filePath), QByteArray("input with spaces\n"));
        params.downloadInput.clear();
        result = handler.download(&params);
        QVERIFY(!result.success);
        QVERIFY(!result.errorMessage.contains("did not finish in time"));
    }

    void testDownloadFailures_data()
    {
        QTest::addColumn<QString>("download");
        QTest::addColumn<int>("timeout");
        QTest::newRow("nonzero exit") << command({"fail"}) << 1000;
        QTest::newRow("missing file") << command({"copy", m_remote + ".missing", "{TEMP_DATABASE}"}) << 1000;
        QTest::newRow("timeout") << command({"sleep"}) << 30;
        QTest::newRow("missing executable") << QString("/nonexistent/easycredentials-remote-test") << 1000;
    }

    void testDownloadFailures()
    {
        QFETCH(QString, download);
        QFETCH(int, timeout);
        auto params = transfer(m_remote);
        params.downloadCommand = download;
        params.downloadTimeoutMsec = timeout;
        const auto beforeLocal = contents(m_local);
        const auto beforeRemote = contents(m_remote);
        DatabaseWidget widget(m_db);
        QVERIFY(!widget.syncWithRemote(params));
        QCOMPARE(contents(m_local), beforeLocal);
        QCOMPARE(contents(m_remote), beforeRemote);
    }

    void testMergeBothDevices()
    {
        auto remote = QSharedPointer<Database>::create();
        QVERIFY(remote->open(m_remote, key()));
        auto* windows = addEntry(m_db, "Windows change");
        auto* mac = addEntry(remote, "Mac change");
        const auto windowsId = windows->uuid();
        const auto macId = mac->uuid();
        QVERIFY(remote->save());
        DatabaseWidget widget(m_db);
        QVERIFY(widget.syncWithRemote(transfer(m_remote)));
        QVERIFY(m_db->rootGroup()->findEntryByUuid(windowsId));
        QVERIFY(m_db->rootGroup()->findEntryByUuid(macId));
        auto reopened = QSharedPointer<Database>::create();
        QVERIFY(reopened->open(m_remote, key()));
        QVERIFY(reopened->rootGroup()->findEntryByUuid(windowsId));
        QVERIFY(reopened->rootGroup()->findEntryByUuid(macId));
        auto local = QSharedPointer<Database>::create();
        QVERIFY(local->open(m_local, key()));
        QVERIFY(local->rootGroup()->findEntryByUuid(macId));
    }

    void testUploadFailurePreservesMergedLocalChanges()
    {
        auto remote = QSharedPointer<Database>::create();
        QVERIFY(remote->open(m_remote, key()));
        const auto macId = addEntry(remote, "Mac change")->uuid();
        const auto windowsId = addEntry(m_db, "Windows change")->uuid();
        QVERIFY(remote->save());
        const auto beforeRemote = contents(m_remote);
        auto params = transfer(m_remote);
        params.uploadCommand = command({"fail"});
        DatabaseWidget widget(m_db);
        QVERIFY(!widget.syncWithRemote(params));
        QCOMPARE(contents(m_remote), beforeRemote);
        auto reopened = QSharedPointer<Database>::create();
        QVERIFY(reopened->open(m_local, key()));
        QVERIFY(reopened->rootGroup()->findEntryByUuid(macId));
        QVERIFY(reopened->rootGroup()->findEntryByUuid(windowsId));
    }

    void testClosingProgressKeepsWindowProtected()
    {
        DatabaseWidget widget(m_db);
        bool protectedAfterClose = false;
        QTimer::singleShot(0, &widget, [&] {
            auto* progress = widget.findChild<QProgressDialog*>();
            QVERIFY(progress);
            progress->close();
            protectedAfterClose = !widget.window()->isEnabled();
        });
        QVERIFY(widget.syncWithRemote(transfer(m_remote)));
        QVERIFY(protectedAfterClose);
        QVERIFY(widget.window()->isEnabled());
    }

    void testInvalidRemote_data()
    {
        QTest::addColumn<bool>("differentKey");
        QTest::newRow("different password") << true;
        QTest::newRow("unrelated database") << false;
    }

    void testInvalidRemote()
    {
        QFETCH(bool, differentKey);
        auto remote = QSharedPointer<Database>::create();
        QVERIFY(remote->open(m_remote, key()));
        if (differentKey) {
            QVERIFY(remote->setKey(key("different")));
        } else {
            remote->rootGroup()->setUuid(QUuid::createUuid());
        }
        QVERIFY(remote->save());
        const auto beforeLocal = contents(m_local);
        const auto beforeRemote = contents(m_remote);
        DatabaseWidget widget(m_db);
        QVERIFY(!widget.syncWithRemote(transfer(m_remote)));
        QCOMPARE(contents(m_local), beforeLocal);
        QCOMPARE(contents(m_remote), beforeRemote);
    }

private:
    void enableAutomatic(const RemoteParams& params)
    {
        RemoteSettings settings(m_db);
        settings.addRemoteParams(new RemoteParams(params));
        settings.setAutomaticSync(params.name, 1);
        settings.saveSettings();
        QVERIFY(settings.automaticSyncParams());
        QVERIFY(m_db->save());
        // Establish an already-synchronized fixture before exercising a particular automatic trigger.
        QVERIFY(QFile::remove(m_remote));
        QVERIFY(QFile::copy(m_local, m_remote));
        settings.setPendingUpload(false);
    }

    static Entry* addEntry(const QSharedPointer<Database>& database, const QString& title)
    {
        auto* entry = new Entry();
        entry->setUuid(QUuid::createUuid());
        entry->setTitle(title);
        entry->setGroup(database->rootGroup());
        return entry;
    }

    QScopedPointer<MainWindow> m_mainWindow;
    QScopedPointer<QTemporaryDir> m_directory;
    QSharedPointer<Database> m_db;
    QString m_local;
    QString m_remote;
};

int main(int argc, char* argv[])
{
    if (argc > 1 && QByteArray(argv[1]) == "--remote-helper") {
        QCoreApplication app(argc, argv);
        return runHelper(app.arguments());
    }
    Application app(argc, argv);
    app.setApplicationName("KeePassXC");
    app.setQuitOnLastWindowClosed(false);
    app.applyTheme();
    TestRemoteSync tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "TestRemoteSync.moc"
