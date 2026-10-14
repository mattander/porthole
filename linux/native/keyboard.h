#pragma once
#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QWidget>

class TouchCheckBox:public QCheckBox {
public:
    using QCheckBox::QCheckBox;
protected:
    bool hitButton(const QPoint &point)const override{return rect().contains(point);}
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
