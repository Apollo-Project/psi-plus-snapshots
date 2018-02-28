#ifndef OPT_ROSTER_MUC_H
#define OPT_ROSTER_MUC_H

#include "optionstab.h"

class QWidget;
class QButtonGroup;

class OptionsTabRosterMuc : public OptionsTab
{
    Q_OBJECT
public:
    OptionsTabRosterMuc(QObject *parent);
    ~OptionsTabRosterMuc();

    QWidget *widget();
    void applyOptions();
    void restoreOptions();

protected:
    virtual void changeEvent(QEvent *e);

private:
    QWidget *w;
};

#endif
