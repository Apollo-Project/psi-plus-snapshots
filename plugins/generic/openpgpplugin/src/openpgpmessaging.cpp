/*
 * Copyright (C) 2020  Boris Pek <tehnick-8@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.     See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "openpgpmessaging.h"
#include "gpgprocess.h"
#include "optionaccessinghost.h"
#include "psiaccountcontrollinghost.h"
#include "stanzasendinghost.h"
#include <QDomElement>

using OpenPgpPluginNamespace::GpgProcess;

void OpenPgpMessaging::setStanzaSendingHost(StanzaSendingHost *host) { m_stanzaSending = host; }

void OpenPgpMessaging::setOptionAccessingHost(OptionAccessingHost *host) { m_optionHost = host; }

void OpenPgpMessaging::setPsiAccountControllingHost(PsiAccountControllingHost *host) { m_accountHost = host; }

bool OpenPgpMessaging::incomingStanza(int account, const QDomElement &stanza)
{
    if (!m_optionHost->getPluginOption("auto-import", true).toBool())
        return false;

    if (stanza.tagName() != "message" && stanza.attribute("type") != "chat")
        return false;

    QString body = stanza.firstChildElement("body").text();

    int start = body.indexOf("-----BEGIN PGP PUBLIC KEY BLOCK-----");
    if (start == -1) {
        return false;
    }

    int end = body.indexOf("-----END PGP PUBLIC KEY BLOCK-----", start);
    if (end == -1) {
        return false;
    }

    QString key = body.mid(start, end - start);

    GpgProcess        gpg;
    const QStringList arguments { "--batch", "--import" };
    gpg.start(arguments);
    gpg.waitForStarted();
    gpg.write(key.toUtf8());
    gpg.closeWriteChannel();
    gpg.waitForFinished();

    QString from = stanza.attribute("from");
    // Cut trash from gpg command output
    QString res = QString::fromUtf8(gpg.readAllStandardError());
    res         = m_stanzaSending->escape(res.mid(0, res.indexOf('\n')));
    res.replace("&quot;", "\"");
    res.replace("&lt;", "<");
    res.replace("&gt;", ">");
    m_accountHost->appendSysMsg(account, from, res);

    // Don't hide message if an error occurred
    if (gpg.exitCode()) {
        return false;
    }

    if (!m_optionHost->getPluginOption("hide-key-message", true).toBool()) {
        return false;
    } else {
        return true;
    }
}

bool OpenPgpMessaging::outgoingStanza(int, QDomElement &) { return false; }

void OpenPgpMessaging::sendPublicKey(int account, const QString &toJid, const QString &keyId, const QString &userId)
{
    const QStringList arguments { "--armor", "--export", "0x" + keyId };

    GpgProcess gpg;
    gpg.start(arguments);
    gpg.waitForFinished();

    if (!gpg.success())
        return;

    const QString &&key = QString::fromUtf8(gpg.readAllStandardOutput());

    m_stanzaSending->sendMessage(account, toJid, key, "", "chat");

    QString res = tr("Public key \"%1\" sent").arg(userId);
    res         = m_stanzaSending->escape(res);
    res.replace("&quot;", "\"");
    res.replace("&lt;", "<");
    res.replace("&gt;", ">");
    m_accountHost->appendSysMsg(account, toJid, res);
}
