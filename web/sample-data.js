/* Fictional, opt-in preview data. No API, storage or device access. */
(function () {
  'use strict';

  window.PortholeSample = function (month, state) {
    var selectedMonth = month || '2026-10';
    var events = [
      event('library', 13, 'Library visit', '4:00 PM', '16:00', 'family', 'Example: after school'),
      event('dinner', 13, 'Dinner together', '6:30 PM', '18:30', 'home', 'Example: at home'),
      event('practice', 14, 'Soccer practice', '5:00 PM', '17:00', 'family', 'Example: community field'),
      event('school', 15, 'School photo day', 'All day', '00:00', 'school', ''),
      event('market', 17, 'Farmers’ market', '9:00 AM', '09:00', 'home', ''),
      event('walk', 18, 'A walk in the woods', '10:30 AM', '10:30', 'family', ''),
      event('reading', 21, 'Reading night', '6:00 PM', '18:00', 'school', ''),
      event('lunch', 25, 'Sunday lunch', '12:30 PM', '12:30', 'family', '')
    ];
    var status = state === 'empty' || state === 'stale' || state === 'unavailable' ? state : 'sample';
    var noData = status === 'empty' || status === 'unavailable';
    return {
      version: 1,
      clock: {
        epochMs: Date.UTC(2026, 9, 13, 9, 41),
        utcOffsetMinutes: 0,
        localDate: '2026-10-13',
        dateLabel: 'Tuesday, October 13',
        timeZoneLabel: 'Local time · sample clock'
      },
      calendar: {
        status: status,
        month: selectedMonth,
        updatedLabel: status === 'stale' ? 'Last updated yesterday, 8:10 PM · sample' : 'Sample calendar · iCloud is not connected',
        events: noData ? [] : events
      },
      upcoming: noData ? [] : events.slice(0, 5).map(function (occurrence) {
        var result = {};
        Object.keys(occurrence).forEach(function (key) { result[key] = occurrence[key]; });
        result.localDate = '2026-10-' + occurrence.localDate.substring(8);
        return result;
      }),
      weather: {
        status: status,
        updatedLabel: status === 'stale' ? 'Last updated yesterday, 8:10 PM · sample forecast' : 'Sample forecast · not live weather',
        locationLabel: 'Example City',
        temperatureLabel: '14°',
        conditionLabel: 'Partly cloudy',
        detailLabel: 'Feels like 12° · Rain 10%',
        icon: 'partly-cloudy',
        forecast: noData ? [] : [
          { dayLabel: 'Wed', highLabel: '16°', lowLabel: '8°', icon: 'sunny' },
          { dayLabel: 'Thu', highLabel: '13°', lowLabel: '7°', icon: 'rain' },
          { dayLabel: 'Fri', highLabel: '12°', lowLabel: '5°', icon: 'cloudy' }
        ]
      }
    };

    function event(id, day, title, time, sortKey, category, location) {
      return {
        id: 'sample-' + id,
        localDate: selectedMonth + '-' + (day < 10 ? '0' : '') + day,
        title: title,
        timeLabel: time,
        sortKey: sortKey,
        allDay: time === 'All day',
        category: category,
        calendarLabel: category === 'family' ? 'Family' : category === 'home' ? 'Home' : 'School',
        location: location
      };
    }
  };
}());
