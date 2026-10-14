#include "calendar-style.h"
#include "data.h"
#include "briefing.h"
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QTextBoundaryFinder>
#include <algorithm>
#include <cmath>

namespace {
double luminance(const QColor &colour) {
    const auto channel=[](double value){return value<=0.04045?value/12.92:std::pow((value+0.055)/1.055,2.4);};
    return 0.2126*channel(colour.redF())+0.7152*channel(colour.greenF())+0.0722*channel(colour.blueF());
}
QColor blend(const QColor &from,const QColor &to,double amount) {
    return QColor::fromRgbF(from.redF()+(to.redF()-from.redF())*amount,
        from.greenF()+(to.greenF()-from.greenF())*amount,
        from.blueF()+(to.blueF()-from.blueF())*amount);
}
struct Source {QString colour,name;};
QMap<QString,Source> sourceMap(const QJsonArray &calendars) {
    QMap<QString,Source> result;
    if(calendars.size()>64)return result;
    for(const auto &value:calendars) {
        if(!value.isObject())return {};
        const auto item=value.toObject();const auto id=item["id"].toString();
        if(id.isEmpty()||id.size()>128||result.contains(id))return {};
        result.insert(id,{canonicalCalendarColour(item["colour"].toString()),item["name"].toString().left(100)});
    }
    return result;
}
struct Phrase {int start,length;QStringList ids;};
std::optional<QList<Phrase>> phrases(const QString &text,const QJsonArray &segments) {
    if(text.isEmpty()||text.toUcs4().size()>BriefingCharacterLimit||segments.isEmpty()||segments.size()>BriefingSegmentLimit)return std::nullopt;
    QList<Phrase> result;QString joined;QTextBoundaryFinder boundary(QTextBoundaryFinder::Grapheme,text);
    for(const auto &value:segments) {
        if(!value.isObject())return std::nullopt;
        const auto item=value.toObject();
        if(!item["text"].isString()||!item["calendar_ids"].isArray())return std::nullopt;
        const auto part=item["text"].toString();const auto refs=item["calendar_ids"].toArray();
        if(part.isEmpty()||refs.size()>BriefingReferencesPerSegmentLimit)return std::nullopt;
        QStringList ids;for(const auto &ref:refs) {
            if(!ref.isString()||ref.toString().isEmpty()||ref.toString().size()>128||ids.contains(ref.toString()))return std::nullopt;
            ids.append(ref.toString());
        }
        const int start=joined.size(),length=part.size();joined+=part;
        if(joined.size()>text.size()||!text.startsWith(joined))return std::nullopt;
        boundary.setPosition(joined.size());if(!boundary.isAtBoundary())return std::nullopt;
        result.append({start,length,ids});
    }
    if(joined!=text)return std::nullopt;
    return result;
}
}
std::optional<QColor> calendarSourceColour(const QString &source) {
    const auto value=canonicalCalendarColour(source);if(value.isEmpty())return std::nullopt;
    const auto red=value.mid(1,2).toInt(nullptr,16),green=value.mid(3,2).toInt(nullptr,16),blue=value.mid(5,2).toInt(nullptr,16);
    const auto alpha=value.size()==9?value.mid(7,2).toInt(nullptr,16):255;
    return QColor::fromRgb(red,green,blue,alpha);
}
QColor compositeCalendarColour(const QColor &source,const QColor &background) {return blend(background,source,source.alphaF());}
double calendarContrast(const QColor &foreground,const QColor &background) {
    const auto first=luminance(foreground),second=luminance(background);
    return (std::max(first,second)+0.05)/(std::min(first,second)+0.05);
}
QColor calendarPhraseTint(const QString &source,const QColor &background,const QColor &foreground) {
    const auto colour=calendarSourceColour(source);if(!colour)return background;
    const auto opaque=compositeCalendarColour(*colour,background);
    // Fade the accent towards the actual surface until ordinary text stays readable.
    // Returning the plain surface is preferable to changing the text colour.
    for(int step=18;step>=0;--step) {
        const auto tint=blend(background,opaque,step/100.0);
        if(calendarContrast(foreground,tint)>=4.5)return tint;
    }
    return background;
}
QList<QTextLayout::FormatRange> briefingFormats(const QString &text,const QJsonArray &segments,
    const QJsonArray &calendars,const QColor &background,const QColor &foreground) {
    const auto ranges=phrases(text,segments);if(!ranges)return {};
    const auto sources=sourceMap(calendars);QList<QTextLayout::FormatRange> result;
    for(const auto &range:*ranges) {
        for(const auto &id:range.ids)if(!sources.contains(id))return {};
        if(range.ids.size()!=1)continue;
        const auto &source=sources[range.ids.first()];if(source.colour.isEmpty())continue;
        QTextLayout::FormatRange format;format.start=range.start;format.length=range.length;
        format.format.setBackground(calendarPhraseTint(source.colour,background,foreground));
        format.format.setForeground(foreground);result.append(format);
    }
    return result;
}
QString briefingSourceDescription(const QString &text,const QJsonArray &segments,const QJsonArray &calendars) {
    const auto ranges=phrases(text,segments);if(!ranges)return {};
    const auto sources=sourceMap(calendars);QStringList lines;
    for(const auto &range:*ranges) {
        QStringList names;
        for(const auto &id:range.ids) {
            if(!sources.contains(id))return {};
            const auto name=sources[id].name;
            if(!name.isEmpty()&&!names.contains(name))names.append(name);
        }
        if(!names.isEmpty())lines.append(text.mid(range.start,range.length).trimmed()+" — "+names.join(", "));
    }
    return lines.join('\n');
}
