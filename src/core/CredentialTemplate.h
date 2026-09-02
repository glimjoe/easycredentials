/*
 *  Copyright (C) 2026 EasyCredentials contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#ifndef KEEPASSX_CREDENTIALTEMPLATE_H
#define KEEPASSX_CREDENTIALTEMPLATE_H

#include <QString>

class Entry;

class CredentialTemplate
{
public:
    enum class Type
    {
        Custom,
        Website,
        Database,
        Ssh,
        Api,
        HttpsCertificate
    };

    static const QString TypeAttribute;
    static const QString DatabaseTypeAttribute;
    static const QString HostAttribute;
    static const QString PortAttribute;
    static const QString DatabaseNameAttribute;
    static const QString SslModeAttribute;
    static const QString FingerprintAttribute;
    static const QString AuthenticationTypeAttribute;
    static const QString HeaderNameAttribute;
    static const QString ScopeAttribute;
    static const QString ExpiresAtAttribute;
    static const QString DomainAttribute;
    static const QString IssuerAttribute;
    static const QString ValidFromAttribute;
    static const QString ValidUntilAttribute;

    static QString typeId(Type type);
    static Type typeFromId(const QString& id);
    static Type typeOf(const Entry* entry);
    static void apply(Entry* entry, Type type);

private:
    CredentialTemplate() = delete;
};

#endif // KEEPASSX_CREDENTIALTEMPLATE_H
