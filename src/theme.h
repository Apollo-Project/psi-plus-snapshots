/*
 * theme.h - base class for any theme
 * Copyright (C) 2010 Justin Karneges, Michail Pishchagin, Rion (Sergey Ilinyh)
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

/*
 MOST IMPORTANT RULE: the theme never depends on thing to which its applied,
 so its one-way relation.
*/

#ifndef PSITHEME_H
#define PSITHEME_H

#include <QObject>
#include <QSharedData>
#include <QHash>
#include <QStringList>
#include <functional>

class QFileInfo;
class ThemePrivate;
class PsiThemeProvider;

//-----------------------------------------------
// Theme
//-----------------------------------------------
class Theme
{
public:
    class ResourceLoader {
        // By default theme does not internal info about its fs.
        // That means rereading zip themes on each file or
        // dir by dir search of each file with name in invalid case.
        // To optimize it, we have this class. It keeps zip opened
        // has a cache of mapping requested names to real.

    public:
        virtual ~ResourceLoader();
        virtual QByteArray loadData(const QString &fileName) = 0;
        virtual bool fileExists(const QString &fileName) = 0;
    };


	Theme();
	Theme(PsiThemeProvider *provider);
	Theme(const Theme &other);
	Theme &operator=(const Theme &other);
	virtual ~Theme();
	bool isValid() const;

	virtual bool exists() = 0;
	virtual bool load(); // synchronous load
	virtual bool load(std::function<void(bool)> loadCallback);  // asynchronous load

    static bool isCompressed(const QFileInfo &); // just tells if theme looks like compressed.
    bool isCompressed();
	// load file from theme in `themePath`
	static QByteArray loadData(const QString &fileName, const QString &themePath, bool caseInsensetive = false);
	QByteArray loadData(const QString &fileName) const;
    ResourceLoader* resourceLoader();

	const QString &id() const;
	void setId(const QString &id);
	const QString &name() const;
	void setName(const QString &name);
	const QString &version() const;
	const QString &description() const;
	const QStringList &authors() const;
	const QString &creation() const;
	const QString &homeUrl() const;

	PsiThemeProvider* themeProvider() const;
	const QString &filePath() const;
	void setFilePath(const QString &f);
	const QHash<QString, QString> info() const;
	void setInfo(const QHash<QString, QString> &i);

	void setCaseInsensitiveFS(bool state);
	bool caseInsensitiveFS() const;

	virtual QString title() const;
	virtual QByteArray screenshot() = 0; // this hack must be replaced with something widget based
private:
	friend class ThemePrivate;
	QExplicitlySharedDataPointer<ThemePrivate> d;
};


#endif
