/*
 *  Copyright (C) 2026 EasyCredentials contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#ifndef KEEPASSX_CREDENTIALFIELDSWIDGET_H
#define KEEPASSX_CREDENTIALFIELDSWIDGET_H

#include <QList>
#include <QPointer>
#include <QWidget>

#include "core/CredentialTemplate.h"

class Entry;
class EntryAttachments;
class EntryAttachmentsWidget;
class EntryAttributes;
class QLabel;
class QFormLayout;
class QLineEdit;
class PasswordWidget;

class CredentialFieldsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CredentialFieldsWidget(QWidget* parent = nullptr);

    void setEntry(Entry* entry,
                  EntryAttributes* attributes,
                  EntryAttachments* attachments,
                  bool readOnly);
    void clear();

public slots:
    void setTitle(const QString& value);
    void setUsername(const QString& value);
    void setPassword(const QString& value);
    void setUrl(const QString& value);

signals:
    void titleChanged(const QString& value);
    void usernameChanged(const QString& value);
    void passwordChanged(const QString& value);
    void urlChanged(const QString& value);
    void modified();
    void errorOccurred(const QString& error);

private:
    enum class Target
    {
        Title,
        Username,
        Password,
        Url,
        Attribute
    };

    struct Binding
    {
        Target target;
        QString attribute;
        QPointer<QWidget> editor;
    };

    void rebuild(CredentialTemplate::Type type);
    void addField(const QString& label,
                  Target target,
                  const QString& attribute = {},
                  const QString& placeholder = {});
    void addPasswordField(const QString& label, Target target);
    void setStandardValue(Target target, const QString& value);
    void refreshAttributes();
    QString editorText(const Binding& binding) const;
    void setEditorText(const Binding& binding, const QString& value);
    void handleEdited(const Binding& binding, const QString& value);

    CredentialTemplate::Type m_type = CredentialTemplate::Type::Custom;
    QPointer<EntryAttributes> m_attributes;
    QList<Binding> m_bindings;
    bool m_loading = false;

    QLabel* const m_descriptionLabel;
    QWidget* const m_formWidget;
    QFormLayout* const m_formLayout;
    QLabel* const m_attachmentsLabel;
    EntryAttachmentsWidget* const m_attachmentsWidget;
};

#endif // KEEPASSX_CREDENTIALFIELDSWIDGET_H
