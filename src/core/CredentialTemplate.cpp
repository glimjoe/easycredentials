/*
 *  Copyright (C) 2026 EasyCredentials contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "CredentialTemplate.h"

#include "core/Entry.h"
#include "core/EntryAttributes.h"

const QString CredentialTemplate::TypeAttribute = QStringLiteral("EasyCredentials.Type");
const QString CredentialTemplate::DatabaseTypeAttribute = QStringLiteral("Database Type");
const QString CredentialTemplate::HostAttribute = QStringLiteral("Host");
const QString CredentialTemplate::PortAttribute = QStringLiteral("Port");
const QString CredentialTemplate::DatabaseNameAttribute = QStringLiteral("Default Database");
const QString CredentialTemplate::SslModeAttribute = QStringLiteral("SSL/TLS Mode");
const QString CredentialTemplate::FingerprintAttribute = QStringLiteral("Fingerprint");
const QString CredentialTemplate::AuthenticationTypeAttribute = QStringLiteral("Authentication Type");
const QString CredentialTemplate::HeaderNameAttribute = QStringLiteral("Header Name");
const QString CredentialTemplate::ScopeAttribute = QStringLiteral("Scope");
const QString CredentialTemplate::ExpiresAtAttribute = QStringLiteral("Expires At");
const QString CredentialTemplate::DomainAttribute = QStringLiteral("Domain");
const QString CredentialTemplate::IssuerAttribute = QStringLiteral("Issuer");
const QString CredentialTemplate::ValidFromAttribute = QStringLiteral("Valid From");
const QString CredentialTemplate::ValidUntilAttribute = QStringLiteral("Valid Until");

QString CredentialTemplate::typeId(Type type)
{
    switch (type) {
    case Type::Website:
        return QStringLiteral("website");
    case Type::Database:
        return QStringLiteral("database");
    case Type::Ssh:
        return QStringLiteral("ssh");
    case Type::Api:
        return QStringLiteral("api");
    case Type::HttpsCertificate:
        return QStringLiteral("https-certificate");
    case Type::Custom:
        return {};
    }
    return {};
}

CredentialTemplate::Type CredentialTemplate::typeFromId(const QString& id)
{
    if (id == QStringLiteral("website")) {
        return Type::Website;
    }
    if (id == QStringLiteral("database")) {
        return Type::Database;
    }
    if (id == QStringLiteral("ssh")) {
        return Type::Ssh;
    }
    if (id == QStringLiteral("api")) {
        return Type::Api;
    }
    if (id == QStringLiteral("https-certificate")) {
        return Type::HttpsCertificate;
    }
    return Type::Custom;
}

CredentialTemplate::Type CredentialTemplate::typeOf(const Entry* entry)
{
    if (!entry) {
        return Type::Custom;
    }
    return typeFromId(entry->attributes()->value(TypeAttribute));
}

void CredentialTemplate::apply(Entry* entry, Type type)
{
    if (!entry) {
        return;
    }

    auto* attributes = entry->attributes();
    if (type == Type::Custom) {
        attributes->remove(TypeAttribute);
        return;
    }

    attributes->set(TypeAttribute, typeId(type));

    switch (type) {
    case Type::Website:
        entry->setTitle(QStringLiteral("Website"));
        break;
    case Type::Database:
        entry->setTitle(QStringLiteral("Database"));
        attributes->set(DatabaseTypeAttribute, {});
        attributes->set(HostAttribute, {});
        attributes->set(PortAttribute, {});
        attributes->set(DatabaseNameAttribute, {});
        attributes->set(SslModeAttribute, {});
        break;
    case Type::Ssh:
        entry->setTitle(QStringLiteral("SSH"));
        attributes->set(HostAttribute, {});
        attributes->set(PortAttribute, QStringLiteral("22"));
        attributes->set(FingerprintAttribute, {});
        break;
    case Type::Api:
        entry->setTitle(QStringLiteral("API"));
        attributes->set(AuthenticationTypeAttribute, QStringLiteral("Bearer"));
        attributes->set(HeaderNameAttribute, QStringLiteral("Authorization"));
        attributes->set(ScopeAttribute, {});
        attributes->set(ExpiresAtAttribute, {});
        break;
    case Type::HttpsCertificate:
        entry->setTitle(QStringLiteral("HTTPS Certificate"));
        attributes->set(DomainAttribute, {});
        attributes->set(FingerprintAttribute, {});
        attributes->set(IssuerAttribute, {});
        attributes->set(ValidFromAttribute, {});
        attributes->set(ValidUntilAttribute, {});
        break;
    case Type::Custom:
        break;
    }
}
