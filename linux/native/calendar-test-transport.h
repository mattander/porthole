#pragma once
#include "caldav.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QPointer>
#include <cstring>

class Reply:public QNetworkReply {
    QByteArray body;qint64 offset=0;
public:
    Reply(const QNetworkRequest &request,QByteArray value,int code,QObject *parent):QNetworkReply(parent),body(value){
        setRequest(request);setUrl(request.url());setAttribute(QNetworkRequest::HttpStatusCodeAttribute,code);open(QIODevice::ReadOnly|QIODevice::Unbuffered);
        if(code>=400)setError(QNetworkReply::ContentAccessDenied,"Fixture error");
        QTimer::singleShot(0,this,[this]{setFinished(true);emit readyRead();emit finished();});
    }
    void abort()override{}
    qint64 bytesAvailable()const override{return body.size()-offset+QIODevice::bytesAvailable();}
    bool isSequential()const override{return true;}
protected:
    qint64 readData(char *data,qint64 max)override{const auto n=qMin(max,body.size()-offset);if(n<=0)return -1;std::memcpy(data,body.constData()+offset,n);offset+=n;return n;}
};
class HangingReply:public QNetworkReply {
public:
    int aborts=0;
    HangingReply(const QNetworkRequest &request,QObject *parent):QNetworkReply(parent){setRequest(request);setUrl(request.url());open(QIODevice::ReadOnly);}
    void abort()override{++aborts;}
    void lateCompletion(){setFinished(true);emit finished();}
protected:
    qint64 readData(char *,qint64)override{return -1;}
};
class FakeCalendar:public QNetworkAccessManager {
public:
    QString token="one",colour="#1a2b3c80";int reports=0,probes=0,probeCode=207,reportCode=207,discoveryCode=207,discoveries=0;bool omitColour=false,hangNext=false;QPointer<HangingReply> hanging;
protected:
    QNetworkReply *createRequest(Operation,const QNetworkRequest &request,QIODevice *output)override{
        if(hangNext){hangNext=false;hanging=new HangingReply(request,this);return hanging;}
        const auto query=output?output->readAll():QByteArray();QByteArray props,colourProperty;int code=207;
        if(!omitColour)colourProperty="<a:calendar-color>"+colour.toUtf8()+"</a:calendar-color>";
        if(query.contains("current-user-principal")){++discoveries;code=discoveryCode;props="<d:current-user-principal><d:href>/principal/</d:href></d:current-user-principal>";}
        else if(query.contains("calendar-home-set"))props="<c:calendar-home-set><d:href>/calendars/</d:href></c:calendar-home-set>";
        else if(query.contains("supported-calendar-component-set"))props="<d:displayname>Fixture</d:displayname><d:resourcetype><c:calendar/></d:resourcetype><c:supported-calendar-component-set><c:comp name='VEVENT'/></c:supported-calendar-component-set>"+colourProperty;
        else if(query.contains("getctag")){++probes;code=probeCode;props="<s:getctag>"+token.toUtf8()+"</s:getctag>"+colourProperty;}
        else {++reports;code=reportCode;props="<c:calendar-data>BEGIN:VCALENDAR\nVERSION:2.0\nBEGIN:VEVENT\nUID:fixture\nDTSTART:20261014T160000Z\nDTEND:20261014T170000Z\nSUMMARY:Fixture "+token.toUtf8()+"\nEND:VEVENT\nEND:VCALENDAR\n</c:calendar-data>";}
        const QByteArray body="<d:multistatus xmlns:d='DAV:' xmlns:c='urn:ietf:params:xml:ns:caldav' xmlns:s='http://calendarserver.org/ns/' xmlns:a='http://apple.com/ns/ical/'><d:response><d:href>/family/</d:href><d:propstat><d:prop>"+props+"</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response></d:multistatus>";
        return new Reply(request,body,code,this);
    }
};
