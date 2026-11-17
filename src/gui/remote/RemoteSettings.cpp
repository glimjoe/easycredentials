/*
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "RemoteSettings.h"

#include "core/Database.h"
#include "core/Config.h"
#include "core/Metadata.h"

#include <QDebug>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

RemoteSettings::RemoteSettings(const QSharedPointer<Database>& db, QObject* parent)
    : QObject(parent)
{
    setDatabase(db);
}

RemoteSettings::~RemoteSettings()
{
    qDeleteAll(m_remoteParams);
}

void RemoteSettings::setDatabase(const QSharedPointer<Database>& db)
{
    qDeleteAll(m_remoteParams);
    m_remoteParams.clear();
    m_db = db;
    loadSettings();
    const auto local = localSettings();
    m_automaticSyncName = local.value("name").toString();
    m_approvedCommandHash = local.value("commandHash").toString();
    m_automaticSyncIntervalMinutes = qBound(1, local.value("intervalMinutes", 5).toInt(), 1440);
}

void RemoteSettings::addRemoteParams(RemoteParams* params)
{
    if (params->name.isEmpty()) {
        qWarning() << "RemoteSettings::addRemoteParams: Remote parameters name is empty";
        delete params;
        return;
    }
    delete m_remoteParams.take(params->name);
    m_remoteParams.insert(params->name, params);
}

void RemoteSettings::removeRemoteParams(const QString& name)
{
    delete m_remoteParams.take(name);
    if (m_automaticSyncName == name) {
        setAutomaticSync({});
    }
}

RemoteParams* RemoteSettings::getRemoteParams(const QString& name) const
{
    if (m_remoteParams.contains(name)) {
        return m_remoteParams.value(name);
    }
    return nullptr;
}

QList<RemoteParams*> RemoteSettings::getAllRemoteParams() const
{
    return m_remoteParams.values();
}

QString RemoteSettings::localSettingsKey() const
{
    if (!m_db || m_db->filePath().isEmpty()) {
        return {};
    }
    const QFileInfo info(m_db->filePath());
    return info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
}

QVariantMap RemoteSettings::localSettings() const
{
    return config()->get(Config::RemoteSyncLocalSettings).toMap().value(localSettingsKey()).toMap();
}

QString RemoteSettings::commandHash(const RemoteParams& params)
{
    QJsonArray values;
    values << params.downloadCommand << params.downloadInput << params.downloadTimeoutMsec
           << params.uploadCommand << params.uploadInput << params.uploadTimeoutMsec;
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(values).toJson(QJsonDocument::Compact),
                                                       QCryptographicHash::Sha256).toHex());
}

const RemoteParams* RemoteSettings::automaticSyncParams() const
{
    const auto* params = getRemoteParams(m_automaticSyncName);
    if (!params || params->downloadCommand.isEmpty() || params->uploadCommand.isEmpty()
        || commandHash(*params) != m_approvedCommandHash) {
        return nullptr;
    }
    return params;
}

int RemoteSettings::automaticSyncIntervalMinutes() const
{
    return m_automaticSyncIntervalMinutes;
}

void RemoteSettings::setAutomaticSync(const QString& name, int intervalMinutes)
{
    m_automaticSyncName = name;
    m_automaticSyncIntervalMinutes = qBound(1, intervalMinutes, 1440);
    const auto* params = getRemoteParams(name);
    m_approvedCommandHash = params ? commandHash(*params) : QString();
}

bool RemoteSettings::hasPendingUpload() const
{
    return localSettings().value("pendingUpload", false).toBool();
}

void RemoteSettings::setPendingUpload(bool pending) const
{
    if (localSettingsKey().isEmpty()) {
        return;
    }
    auto all = config()->get(Config::RemoteSyncLocalSettings).toMap();
    auto local = localSettings();
    local["pendingUpload"] = pending;
    all[localSettingsKey()] = local;
    config()->set(Config::RemoteSyncLocalSettings, all);
    config()->sync();
}

void RemoteSettings::loadSettings()
{
    if (m_db) {
        fromConfig(m_db->metadata()->customData()->value(CustomData::RemoteProgramSettings));
    }
}

void RemoteSettings::saveSettings() const
{
    if (m_db) {
        m_db->metadata()->customData()->set(CustomData::RemoteProgramSettings, toConfig());
        if (!localSettingsKey().isEmpty()) {
            auto all = config()->get(Config::RemoteSyncLocalSettings).toMap();
            auto local = localSettings();
            // A newly approved profile must reconcile saved offline edits, even if the file is currently clean.
            if (!m_automaticSyncName.isEmpty()
                && (local.value("name").toString() != m_automaticSyncName
                    || local.value("commandHash").toString() != m_approvedCommandHash)) {
                local["pendingUpload"] = true;
            }
            local["name"] = m_automaticSyncName;
            local["commandHash"] = m_approvedCommandHash;
            local["intervalMinutes"] = m_automaticSyncIntervalMinutes;
            all[localSettingsKey()] = local;
            config()->set(Config::RemoteSyncLocalSettings, all);
            config()->sync();
        }
    }
}

QString RemoteSettings::toConfig() const
{
    QJsonArray config;
    for (const auto params : m_remoteParams.values()) {
        QJsonObject object;
        object["name"] = params->name;
        object["downloadCommand"] = params->downloadCommand;
        object["downloadCommandInput"] = params->downloadInput;
        object["downloadTimeoutMsec"] = params->downloadTimeoutMsec;
        object["uploadCommand"] = params->uploadCommand;
        object["uploadCommandInput"] = params->uploadInput;
        object["uploadTimeoutMsec"] = params->uploadTimeoutMsec;
        config << object;
    }
    QJsonDocument doc(config);
    return doc.toJson(QJsonDocument::Compact);
}

void RemoteSettings::fromConfig(const QString& data)
{
    qDeleteAll(m_remoteParams);
    m_remoteParams.clear();

    QJsonDocument json = QJsonDocument::fromJson(data.toUtf8());
    for (const auto& item : json.array().toVariantList()) {
        auto itemMap = item.toMap();
        auto* params = new RemoteParams();
        params->name = itemMap["name"].toString();
        params->downloadCommand = itemMap["downloadCommand"].toString();
        params->downloadInput = itemMap["downloadCommandInput"].toString();
        params->downloadTimeoutMsec = itemMap.value("downloadTimeoutMsec", 10000).toInt();
        params->uploadCommand = itemMap["uploadCommand"].toString();
        params->uploadInput = itemMap["uploadCommandInput"].toString();
        params->uploadTimeoutMsec = itemMap.value("uploadTimeoutMsec", 10000).toInt();

        delete m_remoteParams.take(params->name);
        m_remoteParams.insert(params->name, params);
    }
}
