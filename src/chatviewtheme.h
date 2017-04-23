/*
 * chatviewtheme.h - theme for webkit based chatview
 * Copyright (C) 2010 Rion (Sergey Ilinyh)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef CHATVIEWTHEME_H
#define CHATVIEWTHEME_H

#include <QPointer>
#include <functional>

#include "theme.h"
#include "webview.h"

class ChatViewThemeJSUtil;
class ChatViewJSLoader;
class ChatViewThemeProvider;
class ChatViewThemePrivate;
class ChatViewThemeSession;



class ChatViewTheme : public Theme
{
	friend class ChatViewThemeJSUtil;
#ifndef WEBENGINE
	friend class SessionRequestHandler;
#endif
public:

	ChatViewTheme();
	ChatViewTheme(ChatViewThemeProvider *provider);
	ChatViewTheme(const ChatViewTheme &other);
	ChatViewTheme &operator=(const ChatViewTheme &other);
	~ChatViewTheme();

	QByteArray screenshot();

	bool exists();
	bool load(std::function<void(bool)> loadCallback);

	bool isMuc() const;
	inline QUrl baseUrl() const { return QUrl("theme://messages/" + id() + "/"); }

	void putToCache(const QString &key, const QVariant &data);
	void setTransparentBackground(bool enabled = true);
	bool isTransparentBackground() const;
#ifndef WEBENGINE
	void embedSessionJsObject(QSharedPointer<ChatViewThemeSession> session);
#endif
	bool applyToWebView(QSharedPointer<ChatViewThemeSession> session);

	QVariantMap loadFromCacheMulti(const QVariantList &list);
	QVariant cache(const QString &name) const;
private:
	friend class ChatViewJSLoader;
	friend class ChatViewThemePrivate;

	// theme is destroyed only when all chats using it are closed (TODO: ensure)
	QExplicitlySharedDataPointer<ChatViewThemePrivate> cvtd;
};

class ThemeServer;
class ChatViewThemeSession {
	friend class ChatViewTheme;
#ifndef WEBENGINE
	friend class SessionRequestHandler;
#endif

	QString sessId; // unique id of session
	ChatViewTheme theme;
#ifdef WEBENGINE
	ThemeServer *server = 0;
#endif

public:
	virtual ~ChatViewThemeSession();

	inline const QString &sessionId() const { return sessId; }
	virtual WebView* webView() = 0;
	virtual QObject* jsBridge() = 0;
	// returns: data, content-type
	virtual QPair<QByteArray,QByteArray> getContents(const QUrl &url) = 0;
	virtual QString propsAsJsonString() const = 0;
};

#endif
