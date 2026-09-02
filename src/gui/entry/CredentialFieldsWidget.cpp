/*
 *  Copyright (C) 2026 EasyCredentials contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "CredentialFieldsWidget.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "core/Entry.h"
#include "core/EntryAttachments.h"
#include "core/EntryAttributes.h"
#include "gui/PasswordWidget.h"
#include "gui/entry/EntryAttachmentsWidget.h"

CredentialFieldsWidget::CredentialFieldsWidget(QWidget* parent)
    : QWidget(parent)
    , m_descriptionLabel(new QLabel(this))
    , m_formWidget(new QWidget(this))
    , m_formLayout(new QFormLayout(m_formWidget))
    , m_attachmentsLabel(new QLabel(tr("Key and certificate files"), this))
    , m_attachmentsWidget(new EntryAttachmentsWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setObjectName(QStringLiteral("credentialDescriptionLabel"));
    layout->addWidget(m_descriptionLabel);
    layout->addWidget(m_formWidget);
    layout->addWidget(m_attachmentsLabel);
    layout->addWidget(m_attachmentsWidget);
    layout->addStretch();

    m_formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_attachmentsWidget->setButtonsVisible(true);

    connect(m_attachmentsWidget, &EntryAttachmentsWidget::widgetUpdated, this, &CredentialFieldsWidget::modified);
    connect(m_attachmentsWidget,
            &EntryAttachmentsWidget::errorOccurred,
            this,
            &CredentialFieldsWidget::errorOccurred);

    rebuild(CredentialTemplate::Type::Custom);
}

void CredentialFieldsWidget::setEntry(Entry* entry,
                                      EntryAttributes* attributes,
                                      EntryAttachments* attachments,
                                      bool readOnly)
{
    if (m_attributes) {
        m_attributes->disconnect(this);
    }

    m_attributes = attributes;
    m_type = CredentialTemplate::typeOf(entry);
    rebuild(m_type);

    m_attachmentsWidget->linkAttachments(attachments);
    m_attachmentsWidget->setReadOnly(readOnly);
    m_attachmentsWidget->setButtonsVisible(true);

    if (!entry || !attributes) {
        return;
    }

    m_loading = true;
    setStandardValue(Target::Title, entry->title());
    setStandardValue(Target::Username, entry->username());
    setStandardValue(Target::Password, entry->password());
    setStandardValue(Target::Url, entry->url());
    refreshAttributes();
    m_loading = false;

    connect(attributes, &EntryAttributes::customKeyModified, this, [this](const QString&) { refreshAttributes(); });
    connect(attributes, &EntryAttributes::added, this, [this](const QString&) { refreshAttributes(); });
    connect(attributes, &EntryAttributes::removed, this, [this](const QString&) { refreshAttributes(); });
    connect(attributes, &EntryAttributes::reset, this, &CredentialFieldsWidget::refreshAttributes);

    for (const auto& binding : m_bindings) {
        if (auto* lineEdit = qobject_cast<QLineEdit*>(binding.editor.data())) {
            lineEdit->setReadOnly(readOnly);
        } else if (auto* passwordEdit = qobject_cast<PasswordWidget*>(binding.editor.data())) {
            passwordEdit->setReadOnly(readOnly);
            if (!readOnly) {
                passwordEdit->enablePasswordGenerator();
            }
        }
    }
}

void CredentialFieldsWidget::clear()
{
    setEntry(nullptr, nullptr, nullptr, false);
}

void CredentialFieldsWidget::setTitle(const QString& value)
{
    setStandardValue(Target::Title, value);
}

void CredentialFieldsWidget::setUsername(const QString& value)
{
    setStandardValue(Target::Username, value);
}

void CredentialFieldsWidget::setPassword(const QString& value)
{
    setStandardValue(Target::Password, value);
}

void CredentialFieldsWidget::setUrl(const QString& value)
{
    setStandardValue(Target::Url, value);
}

void CredentialFieldsWidget::rebuild(CredentialTemplate::Type type)
{
    while (m_formLayout->rowCount() > 0) {
        m_formLayout->removeRow(0);
    }
    m_bindings.clear();

    bool showAttachments = false;
    switch (type) {
    case CredentialTemplate::Type::Website:
        m_descriptionLabel->setText(tr("Website account"));
        addField(tr("Entry name"), Target::Title);
        addField(tr("Website URL"), Target::Url, {}, tr("https://example.com"));
        addField(tr("Username"), Target::Username);
        addPasswordField(tr("Password"), Target::Password);
        break;
    case CredentialTemplate::Type::Database:
        m_descriptionLabel->setText(tr("Database connection credential"));
        addField(tr("Entry name"), Target::Title);
        addField(tr("Database type"), Target::Attribute, CredentialTemplate::DatabaseTypeAttribute, tr("MySQL"));
        addField(tr("Host"), Target::Attribute, CredentialTemplate::HostAttribute, tr("db.example.com"));
        addField(tr("Port"), Target::Attribute, CredentialTemplate::PortAttribute, tr("3306"));
        addField(tr("Username"), Target::Username);
        addPasswordField(tr("Password"), Target::Password);
        addField(tr("Default database"), Target::Attribute, CredentialTemplate::DatabaseNameAttribute);
        addField(tr("SSL/TLS mode"), Target::Attribute, CredentialTemplate::SslModeAttribute);
        break;
    case CredentialTemplate::Type::Ssh:
        m_descriptionLabel->setText(
            tr("SSH credential. Add private and public key files below; attachments remain encrypted in the KDBX file."));
        addField(tr("Entry name"), Target::Title);
        addField(tr("Host"), Target::Attribute, CredentialTemplate::HostAttribute, tr("server.example.com"));
        addField(tr("Port"), Target::Attribute, CredentialTemplate::PortAttribute, tr("22"));
        addField(tr("Username"), Target::Username);
        addPasswordField(tr("Key passphrase"), Target::Password);
        addField(tr("Fingerprint"), Target::Attribute, CredentialTemplate::FingerprintAttribute);
        showAttachments = true;
        break;
    case CredentialTemplate::Type::Api:
        m_descriptionLabel->setText(tr("API credential"));
        addField(tr("Entry name"), Target::Title);
        addField(tr("Authentication type"),
                 Target::Attribute,
                 CredentialTemplate::AuthenticationTypeAttribute,
                 tr("Bearer"));
        addField(tr("Endpoint"), Target::Url, {}, tr("https://api.example.com"));
        addField(tr("Header name"),
                 Target::Attribute,
                 CredentialTemplate::HeaderNameAttribute,
                 tr("Authorization"));
        addField(tr("Client ID"), Target::Username);
        addPasswordField(tr("API key / token"), Target::Password);
        addField(tr("Scope"), Target::Attribute, CredentialTemplate::ScopeAttribute);
        addField(tr("Expires at"), Target::Attribute, CredentialTemplate::ExpiresAtAttribute, tr("YYYY-MM-DD"));
        break;
    case CredentialTemplate::Type::HttpsCertificate:
        m_descriptionLabel->setText(
            tr("HTTPS certificate credential. Add the certificate, private key, and chain files below."));
        addField(tr("Entry name"), Target::Title);
        addField(tr("Domain"), Target::Attribute, CredentialTemplate::DomainAttribute, tr("example.com"));
        addPasswordField(tr("Private key passphrase"), Target::Password);
        addField(tr("Fingerprint"), Target::Attribute, CredentialTemplate::FingerprintAttribute);
        addField(tr("Issuer"), Target::Attribute, CredentialTemplate::IssuerAttribute);
        addField(tr("Valid from"), Target::Attribute, CredentialTemplate::ValidFromAttribute, tr("YYYY-MM-DD"));
        addField(tr("Valid until"), Target::Attribute, CredentialTemplate::ValidUntilAttribute, tr("YYYY-MM-DD"));
        showAttachments = true;
        break;
    case CredentialTemplate::Type::Custom:
        m_descriptionLabel->clear();
        break;
    }

    m_attachmentsLabel->setVisible(showAttachments);
    m_attachmentsWidget->setVisible(showAttachments);
}

void CredentialFieldsWidget::addField(const QString& label,
                                      Target target,
                                      const QString& attribute,
                                      const QString& placeholder)
{
    auto* editor = new QLineEdit(m_formWidget);
    editor->setPlaceholderText(placeholder);
    QString fieldName = attribute;
    if (fieldName.isEmpty()) {
        switch (target) {
        case Target::Title:
            fieldName = QStringLiteral("title");
            break;
        case Target::Username:
            fieldName = QStringLiteral("username");
            break;
        case Target::Url:
            fieldName = QStringLiteral("url");
            break;
        case Target::Password:
        case Target::Attribute:
            break;
        }
    }
    editor->setObjectName(QStringLiteral("credentialField_%1").arg(fieldName));

    const Binding binding{target, attribute, editor};
    m_bindings.append(binding);
    m_formLayout->addRow(label, editor);

    connect(editor, &QLineEdit::textChanged, this, [this, binding](const QString& value) {
        handleEdited(binding, value);
    });
}

void CredentialFieldsWidget::addPasswordField(const QString& label, Target target)
{
    auto* editor = new PasswordWidget(m_formWidget);
    editor->setShowPassword(false);
    editor->setObjectName(QStringLiteral("credentialPasswordField"));

    const Binding binding{target, {}, editor};
    m_bindings.append(binding);
    m_formLayout->addRow(label, editor);

    connect(editor, &PasswordWidget::textChanged, this, [this, binding](const QString& value) {
        handleEdited(binding, value);
    });
}

void CredentialFieldsWidget::setStandardValue(Target target, const QString& value)
{
    for (const auto& binding : m_bindings) {
        if (binding.target == target) {
            setEditorText(binding, value);
        }
    }
}

void CredentialFieldsWidget::refreshAttributes()
{
    if (!m_attributes) {
        return;
    }

    const bool wasLoading = m_loading;
    m_loading = true;
    for (const auto& binding : m_bindings) {
        if (binding.target == Target::Attribute) {
            setEditorText(binding, m_attributes->value(binding.attribute));
        }
    }
    m_loading = wasLoading;
}

QString CredentialFieldsWidget::editorText(const Binding& binding) const
{
    if (auto* lineEdit = qobject_cast<QLineEdit*>(binding.editor.data())) {
        return lineEdit->text();
    }
    if (auto* passwordEdit = qobject_cast<PasswordWidget*>(binding.editor.data())) {
        return passwordEdit->text();
    }
    return {};
}

void CredentialFieldsWidget::setEditorText(const Binding& binding, const QString& value)
{
    if (editorText(binding) == value) {
        return;
    }

    const QSignalBlocker blocker(binding.editor.data());
    if (auto* lineEdit = qobject_cast<QLineEdit*>(binding.editor.data())) {
        lineEdit->setText(value);
    } else if (auto* passwordEdit = qobject_cast<PasswordWidget*>(binding.editor.data())) {
        passwordEdit->setText(value);
    }
}

void CredentialFieldsWidget::handleEdited(const Binding& binding, const QString& value)
{
    if (m_loading) {
        return;
    }

    switch (binding.target) {
    case Target::Title:
        emit titleChanged(value);
        break;
    case Target::Username:
        emit usernameChanged(value);
        break;
    case Target::Password:
        emit passwordChanged(value);
        break;
    case Target::Url:
        emit urlChanged(value);
        break;
    case Target::Attribute:
        if (m_attributes) {
            m_attributes->set(binding.attribute, value, m_attributes->isProtected(binding.attribute));
        }
        break;
    }

    emit modified();
}
