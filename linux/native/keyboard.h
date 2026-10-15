#pragma once
#include "data.h"
#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QWidget>

class TouchCheckBox:public QCheckBox {
public:
    explicit TouchCheckBox(const QString &text,QWidget *parent=nullptr):QCheckBox(text,parent){
        setMinimumHeight(56);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    }
protected:
    bool hitButton(const QPoint &point)const override{return rect().contains(point);}
    void paintEvent(QPaintEvent *event)override;
};

class CalendarPicker:public QDialog {
public:
    CalendarPicker(const QList<Calendar> &available,const QList<Calendar> &selected,QWidget *parent=nullptr);
    QList<Calendar> chosenCalendars()const;
private:
    struct Choice {Calendar calendar;TouchCheckBox *checkbox;};
    QList<Choice> choices;
};

class TouchKeyboard : public QWidget {
public:
    explicit TouchKeyboard(const QList<QLineEdit*> &fields, QWidget *parent=nullptr);
protected:
    bool eventFilter(QObject *object,QEvent *event) override;
private:
    QPointer<QLineEdit> active;
    QList<QPushButton*> keys;
    bool shifted=false,symbols=false;
    void updateKeys();
};

class SignIn : public QDialog {
public:
    QCheckBox *remember=new TouchCheckBox("Remember this account");
    QLineEdit *account=new QLineEdit;
    QLineEdit *password=new QLineEdit;
    explicit SignIn(QWidget *parent);
    ~SignIn() override;
};

class WifiPassword : public QDialog {
public:
    QLineEdit *password=new QLineEdit;
    explicit WifiPassword(const QString &network,QWidget *parent);
    ~WifiPassword() override;
};
