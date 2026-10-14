#include "keyboard.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace {
QLabel *text(const QString &value,int size) {
    auto *label=new QLabel(value);label->setTextFormat(Qt::PlainText);label->setWordWrap(true);
    QFont font=label->font();font.setPixelSize(size);label->setFont(font);return label;
}
QPushButton *key(const QString &value) {
    auto *button=new QPushButton(value);button->setFocusPolicy(Qt::NoFocus);button->setMinimumHeight(48);button->setMinimumWidth(24);button->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);return button;
}
}

TouchKeyboard::TouchKeyboard(const QList<QLineEdit*> &fields,QWidget *parent):QWidget(parent) {
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(6);
    for(auto *field:fields)field->installEventFilter(this);
    if(!fields.isEmpty())active=fields.first();
    for(const auto &row:QStringList{"1234567890","qwertyuiop","asdfghjkl","zxcvbnm"}) {
        auto *line=new QHBoxLayout;line->setSpacing(6);
        for(auto character:row){
            auto *button=key(QString(character));keys<<button;line->addWidget(button);
            connect(button,&QPushButton::clicked,this,[this,button]{if(active)active->insert(button->text());});
        }
        layout->addLayout(line);
    }
    auto *actions=new QHBoxLayout;
    auto *shift=key("Shift"),*symbolsButton=key("Symbols"),*space=key("Space"),*backspace=key("Backspace");
    shift->setCheckable(true);symbolsButton->setCheckable(true);
    actions->addWidget(shift,1);actions->addWidget(symbolsButton,1);actions->addWidget(space,2);actions->addWidget(backspace,1);
    connect(shift,&QPushButton::clicked,this,[this](bool checked){shifted=checked;updateKeys();});
    connect(symbolsButton,&QPushButton::clicked,this,[this](bool checked){symbols=checked;updateKeys();});
    connect(space,&QPushButton::clicked,this,[this]{if(active)active->insert(" ");});
    connect(backspace,&QPushButton::clicked,this,[this]{if(active)active->backspace();});
    layout->addLayout(actions);
    updateKeys();
}
bool TouchKeyboard::eventFilter(QObject *object,QEvent *event) {
    if(event->type()==QEvent::FocusIn)if(auto *field=qobject_cast<QLineEdit*>(object))active=field;
    return QWidget::eventFilter(object,event);
}
void TouchKeyboard::updateKeys() {
    const QString normal="1234567890qwertyuiopasdfghjklzxcvbnm";
    const QString punctuation="!@#$%^&*()[]{}<>_+-=:;,.?/|\\\"'"+QString(QChar(96))+"~";
    for(int i=0;i<keys.size();++i){
        QString value;
        if(symbols)value=i<punctuation.size()?QString(punctuation[i]):QString();
        else value=shifted?QString(normal[i]).toUpper():QString(normal[i]);
        keys[i]->setText(value);keys[i]->setEnabled(!value.isEmpty());
    }
}

SignIn::SignIn(QWidget *parent):QDialog(parent) {
    setWindowTitle("Connect iCloud");resize(parent?parent->size():QSize(900,760));
    auto *layout=new QVBoxLayout(this);layout->setSpacing(12);
    layout->addWidget(text("Connect iCloud",30));
    layout->addWidget(text("Enter your Apple Account email and a dedicated app-specific password from account.apple.com.",17));
    account->setPlaceholderText("Apple Account email");account->setMaxLength(254);account->setMinimumHeight(48);
    password->setPlaceholderText("xxxx-xxxx-xxxx-xxxx");password->setMaxLength(19);password->setEchoMode(QLineEdit::Password);password->setMinimumHeight(48);
    layout->addWidget(account);layout->addWidget(password);
    auto *show=new TouchCheckBox("Show password");show->setMinimumHeight(44);show->setFocusPolicy(Qt::NoFocus);
    connect(show,&QCheckBox::toggled,this,[this](bool checked){password->setEchoMode(checked?QLineEdit::Normal:QLineEdit::Password);});
    auto *options=new QHBoxLayout;options->addWidget(show);remember->setMinimumHeight(44);remember->setChecked(true);remember->setFocusPolicy(Qt::NoFocus);options->addWidget(remember);layout->addLayout(options);
    layout->addWidget(text("Read-only calendar access. Remembered sign-in is encrypted on this SD card.",15));
    layout->addWidget(new TouchKeyboard({account,password},this));
    auto *actions=new QDialogButtonBox(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    actions->button(QDialogButtonBox::Ok)->setText("Connect iCloud");
    connect(actions,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject);
    layout->addWidget(actions);account->setFocus();
}
SignIn::~SignIn(){password->clear();}

WifiPassword::WifiPassword(const QString &network,QWidget *parent):QDialog(parent) {
    setWindowTitle("Wi-Fi password");resize(parent?parent->size():QSize(900,740));
    auto *layout=new QVBoxLayout(this);layout->setSpacing(12);
    layout->addWidget(text("Join Wi-Fi",30));layout->addWidget(text(network,22));
    password->setPlaceholderText("Wi-Fi password");password->setMaxLength(64);password->setEchoMode(QLineEdit::Password);password->setMinimumHeight(48);
    layout->addWidget(password);
    auto *show=new TouchCheckBox("Show password");show->setMinimumHeight(44);show->setFocusPolicy(Qt::NoFocus);
    connect(show,&QCheckBox::toggled,this,[this](bool checked){password->setEchoMode(checked?QLineEdit::Normal:QLineEdit::Password);});
    layout->addWidget(show);layout->addWidget(new TouchKeyboard({password},this));
    layout->addWidget(text("Linux remembers this network on the SD card for reconnecting. Porthole does not keep a copy of the Wi-Fi password.",15));
    auto *actions=new QDialogButtonBox(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);actions->button(QDialogButtonBox::Ok)->setText("Join network");
    auto *error=text("",15);layout->addWidget(error);
    connect(actions,&QDialogButtonBox::accepted,this,[this,error]{
        const auto value=password->text().toUtf8();
        const bool hex=value.size()==64&&QRegularExpression("^[0-9A-Fa-f]{64}$").match(password->text()).hasMatch();
        if((value.size()>=8&&value.size()<=63)||hex)accept();
        else error->setText("Use 8–63 password bytes, or the network’s 64-digit hexadecimal key.");
    });
    connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject);layout->addWidget(actions);password->setFocus();
}
WifiPassword::~WifiPassword(){password->clear();}
