(function () {
  'use strict';

  var MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
  var WEEKDAYS = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
  var STATUSES = ['ready', 'empty', 'stale', 'unavailable', 'sample'];
  var ICONS = ['sunny', 'partly-cloudy', 'cloudy', 'rain', 'snow', 'unknown'];
  var VIEWS = ['today', 'tomorrow', 'calendar'];
  var query = readQuery();
  var demo = query.demo === '1';
  var demoState = query.state || '';
  var data = null;
  var viewedMonth = '';
  var monthChosen = false;
  var selectedDay = '';
  var calendarMonth = '';
  var currentView = 'today';
  var localToday = '';
  var clockEpoch = 0;
  var clockOffset = 0;
  var clockReceivedAt = 0;
  var request = null;
  var loading = false;
  var failed = false;
  var lastAttemptAt = 0;

  function element(id) { return document.getElementById(id); }
  function setText(id, value) { element(id).textContent = value; }
  function node(tag, className, text) {
    var result = document.createElement(tag);
    if (className) { result.className = className; }
    if (typeof text === 'string') { result.textContent = text; }
    return result;
  }
  function clear(target) { while (target.firstChild) { target.removeChild(target.firstChild); } }
  function pad(number) { return (number < 10 ? '0' : '') + number; }
  function dateKey(date) { return date.getUTCFullYear() + '-' + pad(date.getUTCMonth() + 1) + '-' + pad(date.getUTCDate()); }
  function parseDate(value) {
    var parts = value.split('-');
    return new Date(Date.UTC(parseInt(parts[0], 10), parseInt(parts[1], 10) - 1, parts.length > 2 ? parseInt(parts[2], 10) : 1));
  }
  function dayLabel(key) {
    var date = parseDate(key);
    return WEEKDAYS[date.getUTCDay()] + ', ' + MONTHS[date.getUTCMonth()] + ' ' + date.getUTCDate();
  }
  function overviewDay() {
    if (currentView === 'calendar') { return selectedDay || localToday; }
    if (currentView === 'tomorrow') { return dateKey(new Date(parseDate(localToday).getTime() + 86400000)); }
    return localToday;
  }
  function readQuery() {
    var result = {};
    var pairs = window.location.search.substring(1).split('&');
    var index, parts;
    for (index = 0; index < pairs.length; index += 1) {
      parts = pairs[index].split('=');
      if (parts[0] === 'demo' || parts[0] === 'state') { result[parts[0]] = parts[1] || ''; }
    }
    return result;
  }

  // Reject malformed responses before changing the screen; display text is never HTML.
  function isObject(value) { return value !== null && typeof value === 'object' && !Array.isArray(value); }
  function isText(value) { return typeof value === 'string' && value.length <= 1000; }
  function isFiniteNumber(value) { return typeof value === 'number' && isFinite(value); }
  function isDate(value) {
    if (typeof value !== 'string' || !/^[1-9][0-9]{3}-[0-9]{2}-[0-9]{2}$/.test(value)) { return false; }
    return value >= '1970-01-01' && dateKey(parseDate(value)) === value;
  }
  function isMonth(value) { return typeof value === 'string' && /^[1-9][0-9]{3}-(0[1-9]|1[0-2])$/.test(value) && value >= '1970-01'; }
  function isEvent(value) {
    return isObject(value) && isText(value.id) && isDate(value.localDate) && isText(value.title) &&
      isText(value.timeLabel) && typeof value.sortKey === 'string' && /^([01][0-9]|2[0-3]):[0-5][0-9]$/.test(value.sortKey) &&
      typeof value.allDay === 'boolean' && ['family', 'home', 'school'].indexOf(value.category) !== -1 &&
      (typeof value.location === 'undefined' || isText(value.location)) &&
      (typeof value.calendarLabel === 'undefined' || isText(value.calendarLabel));
  }
  function isEventList(value, limit) { return Array.isArray(value) && value.length <= limit && value.every(isEvent); }
  function isForecast(value) {
    return isObject(value) && isText(value.dayLabel) && isText(value.highLabel) && isText(value.lowLabel) && ICONS.indexOf(value.icon) !== -1;
  }
  function validDashboard(value, expectedMonth) {
    if (!isObject(value) || value.version !== 1 || !isObject(value.clock) || !isObject(value.calendar) || !isObject(value.weather)) { return false; }
    var clock = value.clock;
    var calendar = value.calendar;
    var weather = value.weather;
    var normalizedDate;
    if (!isFiniteNumber(clock.epochMs) || clock.epochMs < 0 || clock.epochMs > 253402214400000 ||
        !isFiniteNumber(clock.utcOffsetMinutes) || clock.utcOffsetMinutes < -840 || clock.utcOffsetMinutes > 840 ||
        !isDate(clock.localDate) || !isText(clock.dateLabel) || !isText(clock.timeZoneLabel)) { return false; }
    normalizedDate = new Date(clock.epochMs + clock.utcOffsetMinutes * 60000);
    if (dateKey(normalizedDate) !== clock.localDate) { return false; }
    if (STATUSES.indexOf(calendar.status) === -1 || !isMonth(calendar.month) || (expectedMonth && calendar.month !== expectedMonth) ||
        !isText(calendar.updatedLabel) || !isEventList(calendar.events, 500) || !isEventList(value.upcoming, 100)) { return false; }
    if ((calendar.status === 'empty' || calendar.status === 'unavailable') && (calendar.events.length || value.upcoming.length)) { return false; }
    if (STATUSES.indexOf(weather.status) === -1 || !isText(weather.updatedLabel) || !isText(weather.locationLabel) ||
        !isText(weather.temperatureLabel) || !isText(weather.conditionLabel) || !isText(weather.detailLabel) ||
        ICONS.indexOf(weather.icon) === -1 || !Array.isArray(weather.forecast) || weather.forecast.length > 5 || !weather.forecast.every(isForecast)) { return false; }
    if ((weather.status === 'empty' || weather.status === 'unavailable') && weather.forecast.length) { return false; }
    return true;
  }

  function sorted(events) {
    return events.slice().sort(function (left, right) {
      var leftKey = left.localDate + left.sortKey;
      var rightKey = right.localDate + right.sortKey;
      return leftKey < rightKey ? -1 : leftKey > rightKey ? 1 : left.title.localeCompare(right.title);
    });
  }
  function activeCalendar() { return data && data.calendar.month === viewedMonth ? data.calendar : null; }
  function eventsForDay(key) {
    var calendar = activeCalendar();
    return calendar ? sorted(calendar.events.filter(function (event) { return event.localDate === key; })) : [];
  }
  function sourceStatus(source, fallback) {
    if (!source) { return loading ? 'Loading…' : fallback; }
    if (failed) { return 'Offline · ' + source.updatedLabel; }
    if (source.status === 'stale') { return 'Stale · ' + source.updatedLabel; }
    if (source.status === 'unavailable') { return 'Unavailable · ' + source.updatedLabel; }
    return source.updatedLabel;
  }

  function renderNotice() {
    var notice = element('notice');
    var sample = demo || data && (data.calendar.status === 'sample' || data.weather.status === 'sample');
    var unavailable = data && (data.calendar.status === 'unavailable' || data.weather.status === 'unavailable');
    var stale = data && (data.calendar.status === 'stale' || data.weather.status === 'stale');
    notice.className = 'notice clearfix';
    element('sample-button').hidden = demo || !!data || loading;
    element('live-link').hidden = !demo;
    if (sample) {
      notice.className += ' sample-notice';
      setText('notice-text', (demo ? 'SAMPLE DATA · Fictional events & weather. iCloud is not connected.' :
        data.calendar.status === 'sample' && data.weather.status === 'sample' ? 'SAMPLE DATA · Fictional events & weather.' :
        data.calendar.status === 'sample' ? 'SAMPLE CALENDAR · Fictional appointments.' : 'SAMPLE WEATHER · Fictional forecast.') +
        (failed ? ' Connection unavailable · Last received data.' : '') +
        (demoState === 'loading' ? ' Loading preview.' : demoState ? ' ' + demoState.charAt(0).toUpperCase() + demoState.substring(1) + ' preview.' : ''));
    } else if (failed) {
      notice.className += ' error-notice';
      setText('notice-text', data ? 'Connection unavailable · Showing the last received data. Time is unverified.' : 'Dashboard unavailable · Check the serving host, then refresh.');
    } else if (loading) {
      setText('notice-text', data ? 'Refreshing the dashboard…' : 'Connecting to your dashboard…');
    } else if (unavailable || stale) {
      notice.className += ' error-notice';
      setText('notice-text', 'Some data is ' + (unavailable ? 'unavailable' : 'stale') + ' · See the source labels below.');
    } else {
      setText('notice-text', '');
    }
    element('refresh-button').disabled = loading && !demo;
  }

  function renderCalendar() {
    var month = parseDate(viewedMonth);
    var first = new Date(month.getTime());
    var last = new Date(Date.UTC(month.getUTCFullYear(), month.getUTCMonth() + 1, 0));
    var weekCount = Math.ceil((month.getUTCDay() + last.getUTCDate()) / 7);
    var body = element('month-days');
    var calendar = activeCalendar();
    var focusDate = document.activeElement && document.activeElement.getAttribute('data-date');
    var index, row, cell, button, date, key, events, dots, eventList, item, count;
    setText('month-title', MONTHS[month.getUTCMonth()] + ' ' + month.getUTCFullYear());
    element('calendar-table').className = 'month-table weeks-' + weekCount;
    first.setUTCDate(1 - month.getUTCDay());
    clear(body);
    for (index = 0; index < weekCount * 7; index += 1) {
      if (index % 7 === 0) { row = node('tr'); body.appendChild(row); }
      date = new Date(first.getTime() + index * 86400000);
      key = dateKey(date);
      events = eventsForDay(key);
      cell = node('td', (index % 7 === 0 || index % 7 === 6 ? 'weekend ' : '') +
        (date.getUTCMonth() !== month.getUTCMonth() ? 'outside-month ' : '') +
        (key === localToday ? 'today ' : '') + (key === selectedDay ? 'selected' : ''));
      button = node('button', 'day-button');
      button.setAttribute('data-date', key);
      button.setAttribute('aria-label', dayLabel(key) + (key === localToday ? ', today' : '') + ', ' +
        (!calendar || calendar.status === 'unavailable' ? 'event data unavailable' : events.length + (events.length === 1 ? ' event' : ' events')));
      button.setAttribute('aria-pressed', key === selectedDay ? 'true' : 'false');
      if (key === localToday) { button.setAttribute('aria-current', 'date'); }
      button.appendChild(node('span', 'day-number', '' + date.getUTCDate()));
      eventList = node('span', 'cell-events');
      dots = node('span', 'event-dots');
      count = Math.min(events.length, 2);
      for (item = 0; item < count; item += 1) { eventList.appendChild(node('span', 'cell-event ' + events[item].category, events[item].title)); }
      if (events.length > count) { eventList.appendChild(node('span', 'event-overflow', '+' + (events.length - count) + ' more')); }
      for (item = 0; item < Math.min(events.length, 3); item += 1) { dots.appendChild(node('i', 'dot ' + events[item].category)); }
      dots.setAttribute('aria-hidden', 'true');
      eventList.setAttribute('aria-hidden', 'true');
      button.appendChild(eventList);
      button.appendChild(dots);
      cell.appendChild(button);
      row.appendChild(cell);
      if (focusDate === key) { button.focus(); }
    }
    element('calendar-table').setAttribute('aria-busy', loading && !calendar ? 'true' : 'false');
    setText('calendar-status', sourceStatus(calendar, failed ? 'Calendar unavailable' : 'No calendar data for this month'));
    element('calendar-status').className = 'source-status' + (failed || calendar && (calendar.status === 'stale' || calendar.status === 'unavailable') ? ' warning' : '');
    element('previous-month').disabled = viewedMonth <= '1970-01';
    element('next-month').disabled = viewedMonth >= '9999-12';
  }

  function renderEmpty(target, title, description) {
    var state = node('div', 'empty-state');
    state.appendChild(node('h3', '', title));
    state.appendChild(node('p', '', description));
    target.appendChild(state);
  }
  function renderAgenda() {
    var target = element('agenda-list');
    var displayDay = overviewDay();
    var calendar = activeCalendar();
    var events = eventsForDay(displayDay);
    var scrollTop = target.scrollTop;
    var index, event, entry, caption;
    var isCalendar = currentView === 'calendar';
    clear(target);
    setText('agenda-title', isCalendar ? 'Day overview' : currentView === 'tomorrow' ? 'Tomorrow' : 'Today');
    if (currentView === 'today') {
      caption = calendar && calendar.status !== 'unavailable' ? events.length + (events.length === 1 ? ' event today' : ' events today') : 'Your day, at a glance.';
    } else {
      caption = dayLabel(displayDay);
    }
    setText('agenda-caption', caption);
    setText('agenda-status', sourceStatus(calendar, failed ? 'Calendar unavailable' : 'Waiting for calendar data'));
    target.setAttribute('aria-label', 'Events for ' + dayLabel(displayDay));
    target.setAttribute('aria-busy', loading && !calendar ? 'true' : 'false');
    if (!events.length) {
      if (loading && !calendar) { renderEmpty(target, 'Getting your days ready…', 'Waiting for the calendar host.'); }
      else if (!calendar || calendar.status === 'unavailable') { renderEmpty(target, 'Calendar unavailable', 'Your appointments will appear when the calendar source is connected.'); }
      else { renderEmpty(target, 'An open day.', currentView === 'tomorrow' ? 'There are no events for tomorrow.' : 'There are no events on this day.'); }
    }
    for (index = 0; index < events.length; index += 1) {
      event = events[index];
      entry = node('article', 'agenda-event ' + event.category);
      entry.appendChild(node('p', 'event-time', event.timeLabel + (event.calendarLabel ? ' · ' + event.calendarLabel : '')));
      entry.appendChild(node('h4', 'event-title', event.title));
      if (event.location) { entry.appendChild(node('p', 'event-location', event.location)); }
      target.appendChild(entry);
    }
    target.scrollTop = scrollTop;
    setText('day-hint', target.scrollHeight > target.clientHeight ? 'Scroll for more events.' : isCalendar ? 'Tap a day for its full overview.' : '');
  }

  function renderWeather() {
    var weather = data && data.weather;
    var unavailable = !weather || weather.status === 'unavailable' || weather.status === 'empty';
    var forecast = element('forecast');
    var index, entry, image, width;
    element('weather').setAttribute('aria-busy', loading && !weather ? 'true' : 'false');
    setText('weather-location', (currentView === 'tomorrow' ? 'Now in ' : '') + (weather ? weather.locationLabel : 'Example City'));
    setText('weather-temperature', unavailable ? '—' : weather.temperatureLabel);
    setText('weather-condition', unavailable ? loading ? 'Loading weather…' : 'Weather unavailable' : weather.conditionLabel);
    setText('weather-detail', unavailable ? 'Forecast will appear when connected.' : weather.detailLabel);
    element('weather-icon').src = 'assets/' + (unavailable ? 'unknown' : weather.icon) + '.svg';
    setText('weather-status', sourceStatus(weather, loading ? 'Waiting for forecast' : 'Forecast unavailable'));
    clear(forecast);
    forecast.className = 'forecast clearfix forecast-' + (weather ? weather.forecast.length : 0);
    if (unavailable || !weather.forecast.length) {
      forecast.appendChild(node('p', 'forecast-empty', loading && !weather ? 'Getting the forecast…' : 'No forecast to show.'));
      return;
    }
    width = 100 / weather.forecast.length;
    for (index = 0; index < weather.forecast.length; index += 1) {
      entry = node('div', 'forecast-day');
      entry.style.width = width + '%';
      entry.appendChild(node('span', 'forecast-name', weather.forecast[index].dayLabel));
      image = node('img');
      image.src = 'assets/' + weather.forecast[index].icon + '.svg';
      image.alt = weather.forecast[index].icon.replace(/-/g, ' ');
      entry.appendChild(image);
      entry.appendChild(node('span', 'forecast-high', weather.forecast[index].highLabel));
      entry.appendChild(node('span', 'forecast-low', weather.forecast[index].lowLabel));
      forecast.appendChild(entry);
    }
  }

  function updateClock() {
    var now = new Date();
    var local = clockReceivedAt ? new Date(clockEpoch + (demo ? 0 : now.getTime() - clockReceivedAt) + clockOffset * 60000) : now;
    var hour = clockReceivedAt ? local.getUTCHours() : local.getHours();
    var minute = clockReceivedAt ? local.getUTCMinutes() : local.getMinutes();
    var key = clockReceivedAt ? dateKey(local) : local.getFullYear() + '-' + pad(local.getMonth() + 1) + '-' + pad(local.getDate());
    var dayChanged = localToday !== key;
    var priorToday = localToday;
    setText('clock', (hour % 12 || 12) + ':' + pad(minute));
    setText('meridiem', hour < 12 ? 'AM' : 'PM');
    if (clockReceivedAt) {
      setText('date-label', data && data.clock.localDate === key ? data.clock.dateLabel : dayLabel(key));
      setText('time-zone', failed ? 'Last host time · unverified' : data ? data.clock.timeZoneLabel : 'Local time · sample clock');
    } else {
      setText('date-label', WEEKDAYS[local.getDay()] + ', ' + MONTHS[local.getMonth()] + ' ' + local.getDate());
      setText('time-zone', 'Device time · unverified');
    }
    localToday = key;
    if (!viewedMonth) { viewedMonth = key.substring(0, 7); }
    if (dayChanged && priorToday && currentView !== 'calendar' && overviewDay().substring(0, 7) !== viewedMonth) {
      viewedMonth = overviewDay().substring(0, 7);
      loadDashboard();
    }
    if (dayChanged && priorToday) { renderCalendar(); renderAgenda(); }
  }
  function render() { updateClock(); renderNotice(); renderCalendar(); renderAgenda(); renderWeather(); }

  function acceptData(result) {
    data = result;
    clockEpoch = result.clock.epochMs;
    clockOffset = result.clock.utcOffsetMinutes;
    clockReceivedAt = new Date().getTime();
    viewedMonth = result.calendar.month;
    loading = false;
    failed = false;
    render();
  }
  function failRequest() { loading = false; failed = true; render(); }
  function cancelRequest() {
    if (!request) { return; }
    request.onreadystatechange = null;
    request.onerror = null;
    request.ontimeout = null;
    request.abort();
    request = null;
    loading = false;
  }
  function loadDashboard() {
    var pending, requestedMonth;
    if (demo) {
      if (demoState === 'loading') { loading = true; failed = false; render(); return; }
      acceptData(window.PortholeSample(viewedMonth, demoState));
      return;
    }
    cancelRequest();
    loading = true;
    lastAttemptAt = new Date().getTime();
    // First response owns the local month, even if the frame's clock is wrong.
    requestedMonth = data || monthChosen ? viewedMonth : '';
    pending = new XMLHttpRequest();
    request = pending;
    pending.open('GET', '/api/dashboard' + (requestedMonth ? '?month=' + encodeURIComponent(requestedMonth) : ''), true);
    pending.timeout = 12000;
    pending.setRequestHeader('Accept', 'application/json');
    pending.onreadystatechange = function () {
      var result;
      if (pending.readyState !== 4 || request !== pending) { return; }
      request = null;
      if (pending.status !== 200) { failRequest(); return; }
      try { result = JSON.parse(pending.responseText); } catch (error) { failRequest(); return; }
      if (!validDashboard(result, requestedMonth)) { failRequest(); return; }
      acceptData(result);
    };
    pending.onerror = function () { if (request === pending) { request = null; failRequest(); } };
    pending.ontimeout = function () { if (request === pending) { request = null; failRequest(); } };
    render();
    pending.send();
  }
  function changeMonth(delta) {
    var month = parseDate(viewedMonth);
    month.setUTCMonth(month.getUTCMonth() + delta);
    viewedMonth = dateKey(month).substring(0, 7);
    monthChosen = true;
    calendarMonth = viewedMonth;
    selectedDay = viewedMonth === localToday.substring(0, 7) ? localToday : viewedMonth + '-01';
    loadDashboard();
  }

  function selectView(view) {
    var isCalendar, index, tab;
    if (VIEWS.indexOf(view) === -1 || view === currentView) { return; }
    cancelRequest();
    if (view === 'calendar' && !calendarMonth) {
      selectedDay = overviewDay();
      calendarMonth = selectedDay.substring(0, 7);
    }
    currentView = view;
    isCalendar = view === 'calendar';
    document.body.className = isCalendar ? 'calendar-view' : 'day-view ' + view + '-view';
    document.title = 'Porthole · ' + (isCalendar ? 'Calendar' : view === 'tomorrow' ? 'Tomorrow' : 'Today');
    element('calendar-pane').hidden = !isCalendar;
    element('conditions').hidden = isCalendar;
    element('weather-status').hidden = isCalendar;
    element('board').setAttribute('aria-labelledby', 'tab-' + view);
    for (index = 0; index < VIEWS.length; index += 1) {
      tab = element('tab-' + VIEWS[index]);
      tab.setAttribute('aria-selected', view === VIEWS[index] ? 'true' : 'false');
      tab.setAttribute('tabindex', view === VIEWS[index] ? '0' : '-1');
    }
    element('agenda-list').scrollTop = 0;
    monthChosen = true;
    viewedMonth = isCalendar ? calendarMonth : overviewDay().substring(0, 7);
    if (!data || data.calendar.month !== viewedMonth) { loadDashboard(); } else { render(); }
  }

  element('previous-month').addEventListener('click', function () { changeMonth(-1); });
  element('next-month').addEventListener('click', function () { changeMonth(1); });
  element('today-button').addEventListener('click', function () {
    viewedMonth = localToday.substring(0, 7);
    monthChosen = true;
    calendarMonth = viewedMonth;
    selectedDay = localToday;
    element('agenda-list').scrollTop = 0;
    loadDashboard();
  });
  element('month-days').addEventListener('click', function (event) {
    var target = event.target;
    while (target && target !== element('month-days') && !target.getAttribute('data-date')) { target = target.parentNode; }
    if (target && target.getAttribute('data-date')) {
      selectedDay = target.getAttribute('data-date');
      element('agenda-list').scrollTop = 0;
      renderCalendar(); renderAgenda();
    }
  });
  element('sample-button').addEventListener('click', function () { window.location.href = '?demo=1'; });
  element('refresh-button').addEventListener('click', loadDashboard);
  VIEWS.forEach(function (view) {
    element('tab-' + view).addEventListener('click', function () { selectView(view); });
  });
  element('view-tabs').addEventListener('keydown', function (event) {
    var index = VIEWS.indexOf(currentView);
    if (event.keyCode === 37) { index = (index + VIEWS.length - 1) % VIEWS.length; }
    else if (event.keyCode === 39) { index = (index + 1) % VIEWS.length; }
    else if (event.keyCode === 36) { index = 0; }
    else if (event.keyCode === 35) { index = VIEWS.length - 1; }
    else { return; }
    event.preventDefault();
    selectView(VIEWS[index]);
    element('tab-' + VIEWS[index]).focus();
  });
  document.addEventListener('visibilitychange', function () {
    if (!document.hidden && !demo) { updateClock(); loadDashboard(); }
  });
  window.addEventListener('pageshow', function (event) { if (event.persisted && !demo) { loadDashboard(); } });
  window.addEventListener('online', function () { if (!demo) { loadDashboard(); } });
  window.setInterval(function () {
    updateClock();
    if (!demo && !document.hidden && !loading && new Date().getTime() - lastAttemptAt >= 60000) { loadDashboard(); }
  }, 1000);

  if (demo) {
    viewedMonth = '2026-10';
    clockEpoch = window.PortholeSample(viewedMonth, '').clock.epochMs;
    clockOffset = -240;
    clockReceivedAt = new Date().getTime();
  }
  updateClock();
  loadDashboard();
}());
