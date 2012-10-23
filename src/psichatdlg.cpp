#include "psichatdlg.h"

#include <QLabel>
#include <QCursor>
#include <QLineEdit>
#include <QToolButton>
#include <QLayout>
#include <QSplitter>
#include <QToolBar>
#include <QPixmap>
#include <QColor>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QContextMenuEvent>
#include <QResizeEvent>
#include <QMenu>
#include <QDragEnterEvent>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>

#include "psicon.h"
#include "psiaccount.h"
#include "iconaction.h"
#include "stretchwidget.h"
#include "psiiconset.h"
#include "iconwidget.h"
#include "fancylabel.h"
#include "msgmle.h"
#include "messageview.h"
#include "iconselect.h"
#include "avatars.h"
#include "activecontactsmenu.h"
#include "psitooltip.h"
#include "psioptions.h"
#include "coloropt.h"
#include "shortcutmanager.h"
#include "accountlabel.h"
#include "iconlabel.h"
#include "capsmanager.h"
#include "psicontactlist.h"
#include "userlist.h"
#include "psicontact.h"
#include "jidutil.h"
#include "xmpp_tasks.h"
#include "lastactivitytask.h"
#include "avcall/avcall.h"
#include "pluginmanager.h"


#define MCMDCHAT		"http://psi-im.org/ids/mcmd#chatmain"



class PsiChatDlg::ChatDlgMCmdProvider : public QObject, public MCmdProviderIface {
	Q_OBJECT
public:
	ChatDlgMCmdProvider(PsiChatDlg *dlg) : dlg_(dlg) {};

	virtual bool mCmdTryStateTransit(MCmdStateIface *oldstate, QStringList command, MCmdStateIface *&newstate, QStringList &preset) {
		Q_UNUSED(preset);
		if (oldstate->getName() == MCMDCHAT) {
			QString cmd;
			if (command.count() > 0) cmd = command[0].toLower();
			if (cmd == "version") {
				JT_ClientVersion *version = new JT_ClientVersion(dlg_->account()->client()->rootTask());
				connect(version, SIGNAL(finished()), SLOT(version_finished()));

				//qDebug() << "querying: " << dlg_->jid().full();
				version->get(dlg_->jid());
				version->go();
				newstate = 0;
			} else if (cmd == "idle") {
				LastActivityTask *idle = new LastActivityTask(dlg_->jid(), dlg_->account()->client()->rootTask());
				connect(idle, SIGNAL(finished()), SLOT(lastactivity_finished()));
				idle->go();
				newstate = 0;
			} else if (cmd == "clear") {
				dlg_->doClear();
				newstate = 0;
			} else if (cmd == "vcard") {
				dlg_->doInfo();
				newstate = 0;
			} else if (cmd == "auth") {
				if (command.count() == 2) {
					if (command[1].toLower() == "request") {
						dlg_->account()->actionAuthRequest(dlg_->jid());
					}
				}
				newstate = 0;
			} else if (cmd == "compact") {
				if (command.count() == 2) {
					QString sub = command[1].toLower();
					if ("on" == sub) {
						dlg_->smallChat_ = true;
					} else if ("off" == sub) {
						dlg_->smallChat_ = false;
					} else {
						dlg_->appendSysMsg("usage: compact {on,off}");
					}
				} else {
					dlg_->smallChat_ = !dlg_->smallChat_;
				}
				dlg_->setLooks();
				newstate = 0;
			} else if (!cmd.isEmpty()) {
				return false;
			}
			return true;
		} else {
			return false;
		}
	};

	virtual QStringList mCmdTryCompleteCommand(MCmdStateIface *state, QString query, QStringList partcommand, int item) {
		Q_UNUSED(partcommand);
		QStringList all;
		if (state->getName() == MCMDCHAT) {
			if (item == 0) {
				all << "version" << "idle" << "clear" << "vcard" << "auth" << "compact";
			}
		}
		QStringList res;
		foreach(QString cmd, all) {
			if (cmd.startsWith(query)) {
				res << cmd;
			}
		}
		return res;
	};

	virtual void mCmdSiteDestroyed() {};
	virtual ~ChatDlgMCmdProvider() {};


public slots:
	void version_finished() {
		JT_ClientVersion *version = qobject_cast<JT_ClientVersion*>(sender());
		if (!version) {
			dlg_->appendSysMsg("No version information available.");
			return;
		}
		dlg_->appendSysMsg(Qt::escape(QString("Version response: N: %2 V: %3 OS: %4")
			.arg(version->name(), version->version(), version->os())));
	};

	void lastactivity_finished()
	{
		LastActivityTask *idle = (LastActivityTask *)sender();

		if (!idle->success()) {
			dlg_->appendSysMsg("Could not determine time of last activity.");
			return;
		}

		if (idle->status().isEmpty()) {
			dlg_->appendSysMsg(QString("Last activity at %1")
				.arg(idle->time().toString()));
		} else {
			dlg_->appendSysMsg(QString("Last activity at %1 (%2)")
				.arg(idle->time().toString(), Qt::escape(idle->status())));
		}
	}

private:
	PsiChatDlg *dlg_;
};



PsiChatDlg::PsiChatDlg(const Jid& jid, PsiAccount* pa, TabManager* tabManager)
	: ChatDlg(jid, pa, tabManager), mCmdManager_(&mCmdSite_), tabCompletion(&mCmdManager_), autoPGP_(true)
{
	connect(account()->psi(), SIGNAL(accountCountChanged()), this, SLOT(updateIdentityVisibility()));
	connect(account(), SIGNAL(addedContact(PsiContact*)), SLOT(updateContactAdding(PsiContact*)));
	connect(account(), SIGNAL(removedContact(PsiContact*)), SLOT(updateContactAdding(PsiContact*)));
	connect(account(), SIGNAL(updateContact(const Jid &)), SLOT(updateContactAdding(const Jid &)));
	mCmdManager_.registerProvider(new ChatDlgMCmdProvider(this));
	tabmode = PsiOptions::instance()->getOption("options.ui.tabs.use-tabs").toBool();
	setWindowBorder(PsiOptions::instance()->getOption("options.ui.decorate-windows").toBool());

	SendButtonTemplatesMenu* menu = getTemplateMenu();
	if (menu) {
		connect(menu, SIGNAL(doPasteAndSend()), this, SLOT(doPasteAndSend()));
		connect(menu, SIGNAL(doEditTemplates()), this, SLOT(editTemplates()));
		connect(menu, SIGNAL(doTemplateText(const QString &)), this, SLOT(sendTemp(const QString &)));
	}
}

PsiChatDlg::~PsiChatDlg()
{
	SendButtonTemplatesMenu* menu = getTemplateMenu();
	if (menu) {
		disconnect(menu, SIGNAL(doPasteAndSend()), this, SLOT(doPasteAndSend()));
		disconnect(menu, SIGNAL(doEditTemplates()), this, SLOT(editTemplates()));
		disconnect(menu, SIGNAL(doTemplateText(const QString &)), this, SLOT(sendTemp(const QString &)));
	}
}

void PsiChatDlg::initUi()
{
	ui_.setupUi(this);

	le_autojid = new ActionLineEdit(ui_.le_jid);
	ui_.le_jid->setLineEdit(le_autojid);
	ui_.le_jid->lineEdit()->setReadOnly(true);
	if (autoSelectContact_) {
		QStringList excl = PsiOptions::instance()->getOption("options.ui.chat.default-jid-mode-ignorelist").toString().toLower().split(",", QString::SkipEmptyParts);
		if (excl.indexOf(jid().bare()) == -1) {
			ui_.le_jid->insertItem(0, "auto", jid().full());
			ui_.le_jid->setCurrentIndex(0);
		} else {
			autoSelectContact_ = false;
		}
	}
	connect(ui_.le_jid, SIGNAL(activated(int)), this, SLOT(contactChanged()));
	UserListItem *ul = account()->findFirstRelevant(jid());
	if (!ul || !ul->isPrivate()) {
		act_autojid = new IconAction(this);
		updateAutojidIcon();
		connect(act_autojid, SIGNAL(triggered()), SLOT(doSwitchJidMode()));
		le_autojid->addAction(act_autojid);
	}

	ui_.lb_ident->setAccount(account());
	ui_.lb_ident->setShowJid(false);

	PsiToolTip::install(ui_.lb_status);
	ui_.lb_status->setPsiIcon(IconsetFactory::iconPtr("status/noauth"));
	setTabIcon(IconsetFactory::iconPtr("status/noauth")->icon());

	ui_.tb_emoticons->setIcon(IconsetFactory::icon("psi/smile").icon());

	connect(ui_.mle, SIGNAL(textEditCreated(QTextEdit*)), SLOT(chatEditCreated()));
	chatEditCreated();

#ifdef Q_WS_MAC
	connect(chatView(), SIGNAL(selectionChanged()), SLOT(logSelectionChanged()));
#endif

	initToolButtons();
	initToolBar();
	updateAvatar();

	PsiToolTip::install(ui_.avatar);

	UserListItem* u = account()->findFirstRelevant(jid());
	if (u && u->isSecure(jid().resource())) {
		setPGPEnabled(true);
	}

	connect(account()->avatarFactory(), SIGNAL(avatarChanged(const Jid&)), this, SLOT(updateAvatar(const Jid&)));

	pm_settings_ = new QMenu(this);
	connect(pm_settings_, SIGNAL(aboutToShow()), SLOT(buildMenu()));
	ui_.tb_actions->setMenu(pm_settings_);
	ui_.tb_actions->setIcon(IconsetFactory::icon("psi/select").icon());
	ui_.tb_actions->setStyleSheet(" QToolButton::menu-indicator { image:none } ");

	connect(account()->capsManager(), SIGNAL(capsChanged(const Jid&)), SLOT(capsChanged(const Jid&)));

	logHeight = PsiOptions::instance()->getOption("options.ui.chat.log-height").toInt();
	chateditHeight = PsiOptions::instance()->getOption("options.ui.chat.chatedit-height").toInt();
	setVSplitterPosition(logHeight, chateditHeight);

	connect(ui_.splitter, SIGNAL(splitterMoved(int,int)), this, SLOT(verticalSplitterMoved(int,int)));

	smallChat_ = PsiOptions::instance()->getOption("options.ui.chat.use-small-chats").toBool();
 	ui_.pb_send->setIcon(IconsetFactory::icon("psi/action_button_send").icon());
	connect(ui_.pb_send, SIGNAL(clicked()), this, SLOT(doSend()));
	connect(ui_.pb_send, SIGNAL(customContextMenuRequested(const QPoint)), SLOT(sendButtonMenu()));

	act_mini_cmd_ = new QAction(this);
	act_mini_cmd_->setText(tr("Input command..."));
	connect(act_mini_cmd_, SIGNAL(triggered()), SLOT(doMiniCmd()));
	addAction(act_mini_cmd_);

	ui_.mini_prompt->hide();

	act_minimize_ = new QAction(this);

	connect(act_minimize_, SIGNAL(triggered()), SLOT(doMinimize()));
	addAction(act_minimize_);

	if (!tabmode) {
		winHeader_ = new PsiWindowHeader(this);
		ui_.vboxLayout1->insertWidget(0, winHeader_);
	}
	setMargins();

	connect(ui_.log->textWidget(), SIGNAL(quote(const QString &)), ui_.mle->chatEdit(), SLOT(insertAsQuote(const QString &)));
}

void PsiChatDlg::verticalSplitterMoved(int, int)
{
	QList<int> list = ui_.splitter->sizes();
	logHeight = list.first();
	chateditHeight = list.last();
	PsiOptions::instance()->setOption("options.ui.chat.log-height", logHeight);
	PsiOptions::instance()->setOption("options.ui.chat.chatedit-height", chateditHeight);

	emit vSplitterMoved(logHeight, chateditHeight);
}

void PsiChatDlg::setVSplitterPosition(int log,int chat)
{
	QList<int> list;
	logHeight = log;
	chateditHeight = chat;
	list << log << chat;
	ui_.splitter->setSizes(list);
}

void PsiChatDlg::updateCountVisibility()
{
	if (PsiOptions::instance()->getOption("options.ui.message.show-character-count").toBool() && !smallChat_) {
		ui_.lb_count->show();
	}
	else {
		ui_.lb_count->hide();
	}
}

void PsiChatDlg::setLooks()
{
	ChatDlg::setLooks();

	const QString css = PsiOptions::instance()->getOption("options.ui.chat.css").toString();
	if (!css.isEmpty()) {
		setStyleSheet(css);
		chatEdit()->setCssString(css);
	}
	ui_.splitter->optionsChanged();
	ui_.mle->optionsChanged();

	int s = PsiIconset::instance()->system().iconSize();
	ui_.lb_status->setFixedSize(s,s);
	ui_.lb_client->setFixedSize(s,s);

	ui_.tb_pgp->hide();
	if (smallChat_) {
		ui_.lb_status->hide();
		ui_.le_jid->hide();
		ui_.tb_actions->hide();
		ui_.tb_emoticons->hide();
		ui_.toolbar->hide();
		ui_.tb_voice->hide();
		ui_.lb_client->hide();
	}
	else {
		ui_.lb_client->show();
		ui_.lb_status->show();
		ui_.le_jid->show();
		if (PsiOptions::instance()->getOption("options.ui.chat.central-toolbar").toBool()) {
			ui_.toolbar->show();
			ui_.tb_actions->hide();
			ui_.tb_emoticons->hide();
			ui_.tb_voice->hide();
		}
		else {
			ui_.toolbar->hide();
			ui_.tb_emoticons->setVisible(PsiOptions::instance()->getOption("options.ui.emoticons.use-emoticons").toBool());
			ui_.tb_actions->show();
			ui_.tb_voice->setVisible(AvCallManager::isSupported());
		}
	}

	updateIdentityVisibility();
	updateCountVisibility();
	updateContactAdding();

	// toolbuttons
	QIcon i;
	i.addPixmap(IconsetFactory::icon("psi/cryptoNo").impix(),  QIcon::Normal, QIcon::Off);
	i.addPixmap(IconsetFactory::icon("psi/cryptoYes").impix(), QIcon::Normal, QIcon::On);
	act_pgp_->setPsiIcon(0);
	act_pgp_->setIcon(i);
}

void PsiChatDlg::setShortcuts()
{
	ChatDlg::setShortcuts();

	act_clear_->setShortcuts(ShortcutManager::instance()->shortcuts("chat.clear"));
// typeahead find bar
	act_find->setShortcuts(ShortcutManager::instance()->shortcuts("chat.find"));
// -- typeahead
	act_info_->setShortcuts(ShortcutManager::instance()->shortcuts("common.user-info"));
	act_history_->setShortcuts(ShortcutManager::instance()->shortcuts("common.history"));

	act_mini_cmd_->setShortcuts(ShortcutManager::instance()->shortcuts("chat.quick-command"));

	act_minimize_->setShortcuts(ShortcutManager::instance()->shortcuts("chat.minimize"));
}

void PsiChatDlg::updateIdentityVisibility()
{
	if (!smallChat_) {
		bool visible = account()->psi()->contactList()->enabledAccounts().count() > 1;
		ui_.lb_ident->setVisible(visible);
	}
	else {
		ui_.lb_ident->setVisible(false);
	}
	
	if (PsiOptions::instance()->getOption("options.ui.disable-send-button").toBool()) {
		ui_.pb_send->hide();
	}
	else {
		ui_.pb_send->show();
	}
}

void PsiChatDlg::updateContactAdding(PsiContact* c)
{
	if (!c || realJid().compare(c->jid(), false)) {
		Jid rj = realJid();
		UserListItem *uli;
		if (rj.isNull() || ((uli = account()->findFirstRelevant(rj)) && (uli->inList() || uli->isSelf()))) {
			act_add_contact->setVisible(false);
		} else {
			act_add_contact->setVisible(true);
		}
	}
}

void PsiChatDlg::updateContactAdding(const Jid &j)
{
	if (realJid().compare(j, false)) {
		updateContactAdding();
	}
}

void PsiChatDlg::initToolButtons()
{
// typeahead find
	QHBoxLayout *hb3a = new QHBoxLayout();
	typeahead = new TypeAheadFindBar(ui_.log->textWidget(), tr("Find toolbar"), 0);
	hb3a->addWidget( typeahead );
	ui_.vboxLayout1->addLayout(hb3a);
// -- typeahead

	act_clear_ = new IconAction(tr("Clear Chat Window"), "psi/clearChat", tr("Clear Chat Window"), 0, this);
	connect(act_clear_, SIGNAL(triggered()), SLOT(doClearButton()));

// typeahead find
	act_find = new IconAction(tr("Find"), "psi/search", tr("&Find"), 0, this, "", true);
	connect(act_find, SIGNAL(triggered()), typeahead, SLOT(toggleVisibility()));
// -- typeahead

	act_html_text = new IconAction(tr("Set Text Format"), "psi/text", tr("Set Text Format"), 0, this);
	connect(act_html_text, SIGNAL(triggered()), chatEdit(), SLOT(doHTMLTextMenu()));

	act_add_contact = new IconAction(tr("Add Contact To Roster"), "psi/addContact", tr("Add Contact"), 0, this);
	connect(act_add_contact, SIGNAL(triggered()), SLOT(addContact()));

	connect(account()->psi()->iconSelectPopup(), SIGNAL(textSelected(QString)), this, SLOT(addEmoticon(QString)));
	act_icon_ = new IconAction(tr("Select Icon"), "psi/smile", tr("Select Icon"), 0, this);
	act_icon_->setMenu(account()->psi()->iconSelectPopup());
	ui_.tb_emoticons->setMenu(account()->psi()->iconSelectPopup());

	act_voice_ = new IconAction(tr("Voice Call"), "psi/avcall", tr("Voice Call"), 0, this);
	connect(act_voice_, SIGNAL(triggered()), SLOT(doVoice()));
	//act_voice_->setEnabled(false);
	ui_.tb_voice->setDefaultAction(act_voice_);

	act_file_ = new IconAction(tr("Send File"), "psi/upload", tr("Send File"), 0, this);
	connect(act_file_, SIGNAL(triggered()), SLOT(doFile()));

	act_pgp_ = new IconAction(tr("Toggle Encryption"), "", tr("Toggle Encryption"), 0, this, 0, true);
	ui_.tb_pgp->setDefaultAction(act_pgp_);
	connect(act_pgp_, SIGNAL(triggered()), SLOT(actPgpToggled()));

	act_info_ = new IconAction(tr("User Info"), "psi/vCard", tr("User Info"), 0, this);
	connect(act_info_, SIGNAL(triggered()), SLOT(doInfo()));

	act_history_ = new IconAction(tr("Message History"), "psi/history", tr("Message History"), 0, this);
	connect(act_history_, SIGNAL(triggered()), SLOT(doHistory()));

	act_compact_ = new IconAction(tr("Toggle Compact/Full Size"), "psi/compact", tr("Toggle Compact/Full Size"), 0, this);
	connect(act_compact_, SIGNAL(triggered()), SLOT(toggleSmallChat()));

	act_ps_ = new IconAction(tr("Paste and Send"), "psi/action_paste_and_send", tr("Paste and Send"), 0, this);
	connect(act_ps_, SIGNAL(triggered()), SLOT(doPasteAndSend()));

	act_templates_ = new IconAction(tr("Templates"), "psi/action_templates", tr("Templates"), 0, this);
	act_templates_->setMenu(getTemplateMenu());

	act_active_contacts = new IconAction (tr("Active contacts"), "psi/jabber", tr("Active contacts"), 0, this);
	connect(act_active_contacts, SIGNAL(triggered()), SLOT(actActiveContacts()));
}

void PsiChatDlg::initToolBar()
{
	ui_.toolbar->setWindowTitle(tr("Chat Toolbar"));
	int s = PsiIconset::instance()->system().iconSize();
	ui_.toolbar->setIconSize(QSize(s, s));
	ui_.toolbar->addAction(act_clear_);
// typeahead find bar
	ui_.toolbar->addAction(act_find);
// -- typeahead
	ui_.toolbar->addAction(act_html_text);
#ifdef PSI_PLUGINS
	PluginManager::instance()->addToolBarButton(this, ui_.toolbar, account(), jid().full());
#endif
	ui_.toolbar->addWidget(new StretchWidget(ui_.toolbar));
	ui_.toolbar->addAction(act_icon_);
	((QToolButton *)ui_.toolbar->widgetForAction(act_icon_))->setPopupMode(QToolButton::InstantPopup);
	ui_.toolbar->addAction(act_templates_);
	((QToolButton *)ui_.toolbar->widgetForAction(act_templates_))->setPopupMode(QToolButton::InstantPopup);
	if (!PsiOptions::instance()->getOption("options.ui.chat.disable-paste-send").toBool())
		ui_.toolbar->addAction(act_ps_);
	ui_.toolbar->addAction(act_file_);
	if (PsiOptions::instance()->getOption("options.pgp.enable").toBool()) {
		ui_.toolbar->addAction(act_pgp_);
	}
	ui_.toolbar->addAction(act_info_);
	ui_.toolbar->addAction(act_history_);
	if (AvCallManager::isSupported()) {
		ui_.toolbar->addAction(act_voice_);
	}
	ui_.toolbar->addAction(act_add_contact);
	ui_.toolbar->addAction(act_active_contacts);
}

void PsiChatDlg::contextMenuEvent(QContextMenuEvent *)
{
	pm_settings_->exec(QCursor::pos());
}

void PsiChatDlg::capsChanged()
{
	ChatDlg::capsChanged();

	QString resource = jid().resource();
	UserListItem *ul = account()->findFirstRelevant(jid());
	if (resource.isEmpty() && ul && !ul->userResourceList().isEmpty()) {
		resource = (*(ul->userResourceList().priority())).name();
	}
	//act_voice_->setEnabled(!account()->capsManager()->isEnabled() || (ul && ul->isAvailable() && account()->capsManager()->features(jid().withResource(resource)).canVoice()));
}

void PsiChatDlg::activated()
{
	ChatDlg::activated();

	updateCountVisibility();

	bool border = PsiOptions::instance()->getOption("options.ui.decorate-windows").toBool();
	if (!PsiOptions::instance()->getOption("options.ui.tabs.use-tabs").toBool()){
		if (!winHeader_.isNull())
			winHeader_->setVisible(!border);
		setWindowBorder(border);
		setMargins();
#if defined(Q_WS_MAC) || defined(Q_WS_HAIKU)
		//
#else
		bringToFront(true);
#endif
	} else {
		if (!winHeader_.isNull())
			winHeader_->setVisible(false);
	}
}

void PsiChatDlg::setContactToolTip(QString text)
{
	ui_.lb_status->setToolTip(text);
	ui_.avatar->setToolTip(text);
}

void PsiChatDlg::updateJidWidget(const QList<UserListItem*> &ul, int status, bool fromPresence)
{
	static bool internal_change = false;
	if (!internal_change) {
		// Filling jid's combobox
		const UserListItem *u = ul.first();
		if (!u)
			return;
		UserResourceList resList = u->userResourceList();
		const QString name = u->name();
		QComboBox *jidCombo = ui_.le_jid;
		if (!u->isPrivate()) {
			// If no conference private chat
			const int combo_idx = jidCombo->currentIndex();
			Jid old_jid = (combo_idx != -1) ? Jid(jidCombo->itemData(combo_idx).toString()) : Jid();
			//if (fromPresence || jid() != old_jid) {
				bool auto_mode = autoSelectContact_;
				Jid new_auto_jid = jid();
				if (auto_mode) {
					if (fromPresence && !resList.isEmpty()) {
						UserResourceList::ConstIterator it = resList.priority();
						new_auto_jid = jid().withResource((*it).name());
					}
				}
				// Filling address combobox
				QString iconStr = "clients/unknown";
				const int resCnt = resList.size();
				if (resCnt == 1) {
					UserResourceList::ConstIterator it = resList.begin();
					if (it != resList.end() && (*it).name().isEmpty())
						// Empty resource,  but online. Transport?
						iconStr = "clients/" + u->findClient((*it).clientName().toLower());
				} else if (resCnt == 0) {
					iconStr = QString();
				}
				setJidComboItem(0, makeContactName(name, u->jid().bare()), u->jid().bare(), iconStr);
				int new_index = -1;
				int curr_index = 1;
				for (UserResourceList::ConstIterator it = resList.begin(); it != resList.end(); it++) {
					UserResource r = *it;
					if (!r.name().isEmpty()) {
						Jid tmp_jid(u->jid().withResource(r.name()));
						QString iconStr2 = "clients/" + u->findClient(r.clientName().toLower());
						setJidComboItem(curr_index, makeContactName(name, tmp_jid), tmp_jid, iconStr2);
						if (new_index == -1 && tmp_jid == new_auto_jid) {
							new_index = curr_index;
						}
						curr_index++;
					}
				}
				if (new_index == -1) {
					new_index = 0;
					if (autoSelectContact_) {
						new_auto_jid = jid().bare();
					} else {
						if (!jid().resource().isEmpty()) {
							new_index = jidCombo->count();
							setJidComboItem(curr_index, makeContactName(name, jid()), jid(), iconStr);
							new_index = curr_index++;
						}
					}
				}
				// Сlean combobox's tail
				while (curr_index < jidCombo->count())
					jidCombo->removeItem(curr_index);

				ui_.le_jid->setCurrentIndex(new_index);
				if (new_auto_jid != jid()) {
					internal_change = true;
					setJid(new_auto_jid);
					if (old_jid != new_auto_jid) {
						if (autoSelectContact_ && (status != XMPP::Status::Offline || !new_auto_jid.resource().isEmpty())) {
							appendSysMsg(tr("Contact has been switched: %1").arg(JIDUtil::toString(new_auto_jid, true)));
						}
					}
				}
			//}
		} else {
			// Conference private chat
			QString iconStr;
			Jid tmp_jid = jid();
			UserResourceList::ConstIterator it = resList.begin();
			if (it != resList.end()) {
				iconStr = "clients/" + u->findClient((*it).clientName().toLower());
				tmp_jid = u->jid().withResource((*it).name());
			} else if (jidCombo->count() > 0) {
				tmp_jid = Jid(jidCombo->itemData(0).toString());
			}
			if (tmp_jid.isValid()) {
				if (iconStr == "clients/unknown")
					iconStr = QString(); // for disable the icon
				setJidComboItem(0, makeContactName(name, tmp_jid), tmp_jid, iconStr);
			}
			// Clean combobox's tail
			while (jidCombo->count() > 1)
				jidCombo->removeItem(1);
			//-
			jidCombo->setCurrentIndex(0);
		}
		jidCombo->setToolTip(jidCombo->currentText());
	}
	internal_change = false;
}

void PsiChatDlg::actActiveContacts()
{
	ActiveContactsMenu* acm = new ActiveContactsMenu(account()->psi(), this);
	if(!acm->actions().isEmpty())
		acm->exec(QCursor::pos());
	delete acm;
}

void PsiChatDlg::contactUpdated(UserListItem* u, int status, const QString& statusString)
{
	Q_UNUSED(statusString);

	if (status == -1 || !u) {
		ui_.lb_status->setPsiIcon(IconsetFactory::iconPtr("status/noauth"));
		setTabIcon(IconsetFactory::iconPtr("status/noauth")->icon());
	}
	else {
		ui_.lb_status->setPsiIcon(PsiIconset::instance()->statusPtr(jid(), status));
		setTabIcon(PsiIconset::instance()->statusPtr(jid(), status)->icon());
	}

	if (u) {
		setContactToolTip(u->makeTip(true, false));
	}
	else {
		setContactToolTip(QString());
	}

	if (u) {
		UserResourceList srl = u->userResourceList();
		if(!srl.isEmpty()) {			
			UserResource r;
			if(!jid().resource().isEmpty()) {
				QString res = jid().resource();
				UserResourceList::ConstIterator it = srl.find(res);
				if(it != srl.end())
					r = *it;
			}
			if(r.clientName().isEmpty()) {
				srl.sort();
				r = srl.first();
			}
			const QPixmap &pix = IconsetFactory::iconPixmap("clients/" + u->findClient(r.clientName().toLower()) );
			ui_.lb_client->setPixmap(pix);
			ui_.lb_client->setToolTip(r.versionString());
		}
	}
}

void PsiChatDlg::updateAvatar()
{
	QString res;
	QString client;

	if (!PsiOptions::instance()->getOption("options.ui.chat.avatars.show").toBool()) {
		ui_.avatar->hide();
		return;
	}

	UserListItem *ul = account()->findFirstRelevant(jid());
	bool private_ = false;
	if (ul && !ul->userResourceList().isEmpty()) {
		UserResourceList::Iterator it = ul->userResourceList().find(jid().resource());
		if (it == ul->userResourceList().end())
			it = ul->userResourceList().priority();

		res = (*it).name();
		client = (*it).clientName();
		private_ = ul->isPrivate();
	}
	//QPixmap p = account()->avatarFactory()->getAvatar(jid().withResource(res),client);
	QPixmap p = private_ ?
			account()->avatarFactory()->getMucAvatar(jid().withResource(res)) :
			account()->avatarFactory()->getAvatar(jid().withResource(res));
	if (p.isNull()) {
		if (!PsiOptions::instance()->getOption("options.ui.contactlist.avatars.use-default-avatar").toBool()) {
			ui_.avatar->hide();
			return;
		}
		p = IconsetFactory::iconPixmap("psi/default_avatar");
	}
	int optSize = PsiOptions::instance()->getOption("options.ui.chat.avatars.size").toInt();
	ui_.avatar->setFixedSize(optSize, optSize);
	int avatarSize = p.width(); //qMax(p.width(), p.height());
	if (avatarSize > optSize)
		avatarSize = optSize;
	ui_.avatar->setPixmap(p.scaled(QSize(avatarSize, avatarSize), Qt::KeepAspectRatio, Qt::SmoothTransformation));
	ui_.avatar->show();
}

void PsiChatDlg::optionsUpdate()
{
	smallChat_ = PsiOptions::instance()->getOption("options.ui.chat.use-small-chats").toBool();

	ChatDlg::optionsUpdate();
// typeahead find bar
	typeahead->optionsUpdate();
}

void PsiChatDlg::updatePGP()
{
	if (account()->hasPGP()) {
		act_pgp_->setEnabled(true);
	}
	else {
		setPGPEnabled(false);
		act_pgp_->setEnabled(false);
	}

	checkPGPAutostart();

	ui_.tb_pgp->setVisible(account()->hasPGP() &&
	                       !smallChat_ &&
	                       !PsiOptions::instance()->getOption("options.ui.chat.central-toolbar").toBool());

	ui_.log->setEncryptionEnabled(isEncryptionEnabled());
}

void PsiChatDlg::checkPGPAutostart()
{
	if(account()->hasPGP() && autoPGP_ && PsiOptions::instance()->getOption("options.pgp.auto-start").toBool()) {
		UserListItem *item = account()->findFirstRelevant(jid());
		if(item && !item->publicKeyID().isEmpty()) {
			if(!jid().resource().isEmpty()) {
				UserResourceList::Iterator rit = item->userResourceList().find(jid().resource());
				if(rit !=item->userResourceList().end()) {
					UserResource r = *rit;
					if(r.pgpVerifyStatus() != 0) {
						setPGPEnabled(false);
						return;
					}
				}
			}
			setPGPEnabled(true);
		}
	}
}

void PsiChatDlg::actPgpToggled()
{
	autoPGP_ = false;
}

void PsiChatDlg::doClearButton()
{
	if (PsiOptions::instance()->getOption("options.ui.chat.warn-before-clear").toBool()) {
		switch (
			QMessageBox::warning(
				this,
				tr("Warning"),
				tr("Are you sure you want to clear the chat window?\n(note: does not affect saved history)"),
				QMessageBox::Yes, QMessageBox::YesAll, QMessageBox::No
			)
		) {
		case QMessageBox::No:
			break;
		case QMessageBox::YesAll:
			PsiOptions::instance()->setOption("options.ui.chat.warn-before-clear", false);
			// fall-through
		case QMessageBox::Yes:
			doClear();
		}
	} else {
		doClear();
	}
}

void PsiChatDlg::setPGPEnabled(bool enabled)
{
	act_pgp_->setChecked(enabled);
	ui_.log->setEncryptionEnabled(enabled);
}

void PsiChatDlg::toggleSmallChat()
{
	smallChat_ = !smallChat_;
	setLooks();
}

void PsiChatDlg::buildMenu()
{
	// Dialog menu
	pm_settings_->clear();
	pm_settings_->addAction(act_compact_);
	pm_settings_->addAction(act_clear_);
	pm_settings_->addSeparator();

	pm_settings_->addAction(act_icon_);
	pm_settings_->addAction(act_templates_);
	if (!PsiOptions::instance()->getOption("options.ui.chat.disable-paste-send").toBool())
		pm_settings_->addAction(act_ps_);
	pm_settings_->addAction(act_file_);
	if (AvCallManager::isSupported()) {
		pm_settings_->addAction(act_voice_);
	}
	pm_settings_->addAction(act_pgp_);
	pm_settings_->addSeparator();

	pm_settings_->addAction(act_info_);
	pm_settings_->addAction(act_history_);
#ifdef PSI_PLUGINS
	if(!PsiOptions::instance()->getOption("options.ui.chat.central-toolbar").toBool()) {
		pm_settings_->addSeparator();
		PluginManager::instance()->addToolBarButton(this, pm_settings_, account(), jid().full());
	}
#endif
}

void PsiChatDlg::updateCounter()
{
	ui_.lb_count->setNum(chatEdit()->toPlainText().length());
}

bool PsiChatDlg::isEncryptionEnabled() const
{
	return act_pgp_->isChecked();
}

void PsiChatDlg::appendSysMsg(const QString &str)
{
	chatView()->dispatchMessage(MessageView::fromHtml(str, MessageView::System));
}

ChatView* PsiChatDlg::chatView() const
{
	return ui_.log;
}

ChatEdit* PsiChatDlg::chatEdit() const
{
	return ui_.mle->chatEdit();
}

void PsiChatDlg::chatEditCreated()
{
	ChatDlg::chatEditCreated();

	connect(chatEdit(), SIGNAL(textChanged()), this, SLOT(updateCounter()));
	chatEdit()->installEventFilter(this);

	mCmdSite_.setInput(chatEdit());
	mCmdSite_.setPrompt(ui_.mini_prompt);
	tabCompletion.setTextEdit(chatEdit());
}


void PsiChatDlg::doSend() {
	tabCompletion.reset();
	if (mCmdSite_.isActive()) {
		QString str = chatEdit()->toPlainText();
		if (!mCmdManager_.processCommand(str)) {
			appendSysMsg(tr("Error: Can not parse command: ") + Qt::escape(str));
		}
	} else {
		ChatDlg::doSend();
	}
}

void PsiChatDlg::doMiniCmd()
{
	mCmdManager_.open(new MCmdSimpleState(MCMDCHAT, tr("Command>")), QStringList() );
}

void PsiChatDlg::addContact()
{
	Jid j(realJid());
	UserListItem *uli = account()->findFirstRelevant(jid());
	QString name = uli && !uli->name().isEmpty()? uli->name() : j.node();
	account()->openAddUserDlg(j.withResource(""), name.isEmpty()?j.node():name, "");
}

void PsiChatDlg::doMinimize()
{
	window()->showMinimized();
}

bool PsiChatDlg::eventFilter( QObject *obj, QEvent *ev ) {
	if ( obj == chatEdit() && ev->type() == QEvent::KeyPress ) {
		QKeyEvent *e = (QKeyEvent *)ev;

		if ( e->key() == Qt::Key_Tab ) {
			tabCompletion.tryComplete();
			return true;
		}

		tabCompletion.reset();
	}

	return ChatDlg::eventFilter( obj, ev );
}

void PsiChatDlg::sendButtonMenu()
{
	SendButtonTemplatesMenu* menu = getTemplateMenu();
	if (menu) {
		menu->setParams(true);
		menu->exec(QCursor::pos());
		menu->setParams(false);
		chatEdit()->setFocus();
	}
}

void PsiChatDlg::editTemplates()
{
	if(ChatDlg::isActiveTab()) {
		showTemplateEditor();
	}
}

void PsiChatDlg::doPasteAndSend()
{
	if(ChatDlg::isActiveTab()) {
		chatEdit()->paste();
		doSend();
		act_ps_->setEnabled(false);
		QTimer::singleShot(2000, this, SLOT(psButtonEnabled()));
	}
}

void PsiChatDlg::psButtonEnabled()
{
	act_ps_->setEnabled(true);
}

void PsiChatDlg::sendTemp(const QString &templText)
{
	if(ChatDlg::isActiveTab()) {
		if (!templText.isEmpty()) {
			chatEdit()->textCursor().insertText(templText);
			if (!PsiOptions::instance()->getOption("options.ui.chat.only-paste-template").toBool())
				doSend();
		}
	}
}

void PsiChatDlg::setMargins()
{
	ui_.vboxLayout->setContentsMargins(0,0,0,0);
	ui_.vboxLayout2->setContentsMargins(4,0,4,4);
	if (!tabmode) {
		ui_.hboxLayout->setContentsMargins(4,0,4,0);
		if (!isBorder()) {
			ui_.vboxLayout1->setContentsMargins(0,0,0,0);
		}
		else {
			ui_.vboxLayout1->setContentsMargins(0,4,0,0);
		}
	}
	else {
		ui_.vboxLayout1->setContentsMargins(4,4,4,0);
		ui_.hboxLayout->setContentsMargins(2,0,4,0);
	}
}

QString PsiChatDlg::makeContactName(const QString &name, const Jid &jid) const
{
	QString name_;
	QString j = JIDUtil::toString(jid, true);
	if (!name.isEmpty())
		name_ = name + QString(" <%1>").arg(j);
	else
		name_ = j;
	return name_;
}

void PsiChatDlg::contactChanged()
{
	int curr_index = ui_.le_jid->currentIndex();
	Jid jid_(ui_.le_jid->itemData(curr_index).toString());
	if (jid_ != jid()) {
		autoSelectContact_ = false;
		setJid(jid_);
		updateAutojidIcon();
	}
}

void PsiChatDlg::updateAutojidIcon()
{
	QIcon icon(IconsetFactory::iconPixmap("psi/autojid"));
	QPixmap pix;
	QString text;
	if (autoSelectContact_) {
		pix = icon.pixmap(QSize(16, 16), QIcon::Normal, QIcon::Off);
		text = tr("turn off autojid");
	} else {
		pix = icon.pixmap(QSize(16, 16), QIcon::Disabled, QIcon::Off);
		text = tr("turn on autojid");
	}
	act_autojid->setIcon(QIcon(pix));
	act_autojid->setText(text);
	act_autojid->setToolTip(text);
	act_autojid->setStatusTip(text);
}

void PsiChatDlg::setJidComboItem(int pos, const QString &text, const Jid &jid, const QString &icon_str)
{
	// Warning! If pos >= items count, the element will be added in a list tail
	//-
	QIcon icon;
	QComboBox *jid_combo = ui_.le_jid;
	if (!icon_str.isEmpty()) {
		const PsiIcon picon = IconsetFactory::icon(icon_str);
		icon = picon.icon();
	}
	if (jid_combo->count() > pos) {
		jid_combo->setItemText(pos, text);
		jid_combo->setItemData(pos, JIDUtil::toString(jid, true));
		jid_combo->setItemIcon(pos, icon);
	} else {
		jid_combo->addItem(icon, text, JIDUtil::toString(jid, true));
	}
}

void PsiChatDlg::doSwitchJidMode()
{
	autoSelectContact_ = ! autoSelectContact_;
	updateAutojidIcon();
	if (autoSelectContact_) {
		const QList<UserListItem*> ul = account()->findRelevant(jid().bare());
		UserStatus userStatus = userStatusFor(jid(), ul, false);
		updateJidWidget(ul, userStatus.statusType, true);
		userStatus = userStatusFor(jid(), ul, false);
		contactUpdated(userStatus.userListItem, userStatus.statusType, userStatus.status);
	}
}

#include "psichatdlg.moc"
