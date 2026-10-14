// Minimal client for the Liftosaur "Running a Workout" API.
// https://www.liftosaur.com/doc/api

var BASE_URL = 'https://www.liftosaur.com/api/v1';
var CLIENT = 'pebblesaur/1.0.0';
var TIMEOUT_MS = 20000;

function LiftosaurError(status, code, message) {
  this.name = 'LiftosaurError';
  this.status = status;
  this.code = code;
  this.message = message;
}
LiftosaurError.prototype = Object.create(Error.prototype);
LiftosaurError.prototype.constructor = LiftosaurError;

function friendlyMessage(status, code, message) {
  if (status === 0) {
    return 'No connection to Liftosaur.';
  }
  if (status === 401) {
    return 'Liftosaur rejected the API key. Check it in Pebblesaur settings.';
  }
  if (status === 403) {
    return 'The Liftosaur API needs a premium subscription.';
  }
  switch (code) {
    case 'missing_set_input':
      return 'This set needs more input. Log it in the Liftosaur app.';
    case 'set_not_found':
    case 'entry_not_found':
      return 'The workout changed on your phone. Refreshing.';
    case 'workout_already_active':
      return 'Another workout is already in progress.';
    case 'no_active_workout':
      return 'No workout in progress.';
    case 'program_error':
      return 'Program script error: ' + (message || 'check your program in Liftosaur.');
  }
  return message || ('Liftosaur error ' + status);
}

// options: { apiKey, deviceId, baseUrl }
function Client(options) {
  this.apiKey = options.apiKey;
  this.deviceId = options.deviceId;
  this.baseUrl = options.baseUrl || BASE_URL;
}

// callback(err, data) where data is the response's "data" field.
Client.prototype.request = function (method, path, body, callback) {
  var xhr = new XMLHttpRequest();
  var done = false;
  function finish(err, data) {
    if (!done) {
      done = true;
      callback(err, data);
    }
  }
  xhr.open(method, this.baseUrl + path, true);
  xhr.timeout = TIMEOUT_MS;
  xhr.setRequestHeader('Authorization', 'Bearer ' + this.apiKey);
  xhr.setRequestHeader('Accept', 'application/json');
  if (method !== 'GET') {
    // Liftosaur requires both on every write.
    xhr.setRequestHeader('X-Liftosaur-Device-Id', this.deviceId);
    xhr.setRequestHeader('X-Liftosaur-Client', CLIENT);
    xhr.setRequestHeader('Content-Type', 'application/json');
  }
  xhr.onload = function () {
    var json = null;
    try {
      json = xhr.responseText ? JSON.parse(xhr.responseText) : null;
    } catch (e) {
      json = null;
    }
    if (xhr.status >= 200 && xhr.status < 300) {
      finish(null, json && json.data);
      return;
    }
    var error = (json && json.error) || {};
    finish(new LiftosaurError(xhr.status, error.code, friendlyMessage(xhr.status, error.code, error.message)));
  };
  xhr.onerror = function () {
    finish(new LiftosaurError(0, 'network', friendlyMessage(0)));
  };
  xhr.ontimeout = xhr.onerror;
  xhr.send(body == null ? null : JSON.stringify(body));
};

function workoutCallback(callback) {
  return function (err, data) {
    callback(err, err ? null : (data && data.workout) || null);
  };
}

Client.prototype.currentWorkout = function (callback) {
  this.request('GET', '/workout/current', null, workoutCallback(callback));
};

Client.prototype.nextWorkout = function (callback) {
  this.request('GET', '/workout/next', null, workoutCallback(callback));
};

// preview: the workout from nextWorkout(), so we start the same day we showed.
Client.prototype.startWorkout = function (preview, callback) {
  var body = {};
  if (preview && preview.programId && preview.dayData) {
    body.programId = preview.programId;
    body.week = preview.dayData.week;
    body.dayInWeek = preview.dayData.dayInWeek;
  }
  this.request('POST', '/workout/start', body, workoutCallback(callback));
};

Client.prototype.logSet = function (entryId, setId, completed, callback) {
  this.request('POST', '/workout/set', {
    entryId: entryId,
    setId: setId,
    completed: completed
  }, workoutCallback(callback));
};

Client.prototype.finishWorkout = function (startTime, callback) {
  this.request('POST', '/workout/finish', { startTime: startTime }, workoutCallback(callback));
};

module.exports = {
  Client: Client,
  LiftosaurError: LiftosaurError,
  BASE_URL: BASE_URL,
  CLIENT: CLIENT
};
