var test = require('node:test');
var assert = require('node:assert');
var protocol = require('../src/pkjs/protocol');
var workout = require('./fixtures/workout.json');

// Also parsed by test/c/test_model.c: keep the two in sync.
var SQUAT_SETS = '1,5,-1,90,-1,-1,-1,60kg;16,5,-1,180,6,-1,85,102.5kg;6,5,3,180,-1,90,-1,100kg';
var PLANK_SETS = '96,1,-1,60,-1,-1,-1,';

test('encodeWorkout sends a header, one message per exercise, then done', function () {
  var messages = protocol.encodeWorkout(workout, true);
  assert.deepStrictEqual(messages[0], {
    MsgType: protocol.MsgType.WORKOUT,
    Started: 1,
    Count: 2,
    ProgramName: 'GZCLP',
    DayName: 'A1'
  });
  assert.deepStrictEqual(messages[1], {
    MsgType: protocol.MsgType.ENTRY,
    EntryIndex: 0,
    Name: 'Squat',
    Info: 'T1 - work up to a 5RM',
    Sets: SQUAT_SETS
  });
  assert.strictEqual(messages[2].Sets, PLANK_SETS);
  assert.strictEqual(messages[2].Info, '');
  assert.deepStrictEqual(messages[3], { MsgType: protocol.MsgType.DONE });
});

test('encodeWorkout caps exercises and sets at the watch limits', function () {
  var entry = JSON.parse(JSON.stringify(workout.entries[0]));
  entry.warmupSets = [];
  entry.sets = [];
  for (var i = 0; i < 20; i++) {
    entry.sets.push(workout.entries[0].sets[1]);
  }
  var big = { programName: 'P', dayName: 'D', entries: [] };
  for (var j = 0; j < 20; j++) {
    big.entries.push(entry);
  }
  var messages = protocol.encodeWorkout(big, false);
  assert.strictEqual(messages[0].Count, protocol.Limits.ENTRIES);
  assert.strictEqual(messages.length, protocol.Limits.ENTRIES + 2);
  assert.strictEqual(messages[1].Sets.split(';').length, protocol.Limits.SETS);
});

test('truncateUtf8 respects byte limits without splitting characters', function () {
  assert.strictEqual(protocol.truncateUtf8('abcdef', 4), 'abc');
  // "é" is 2 bytes: 3 bytes of room fits one, not one and a half.
  assert.strictEqual(protocol.truncateUtf8('ééé', 4), 'é');
  assert.strictEqual(protocol.truncateUtf8('a💪b', 6), 'a💪');
  assert.strictEqual(protocol.truncateUtf8('a💪b', 5), 'a');
  assert.strictEqual(protocol.truncateUtf8(null, 4), '');
});

test('formatWeight handles strings, objects and numbers', function () {
  assert.strictEqual(protocol.formatWeight('100 kg'), '100kg');
  assert.strictEqual(protocol.formatWeight({ value: 225, unit: 'lb' }), '225lb');
  assert.strictEqual(protocol.formatWeight(50), '50');
  assert.strictEqual(protocol.formatWeight(null), '');
});

test('locateSet counts warmups first', function () {
  assert.strictEqual(protocol.locateSet(workout, 0, 0).set.setId, 'warmaa');
  assert.strictEqual(protocol.locateSet(workout, 0, 2).set.setId, 'asdfgh');
  assert.strictEqual(protocol.locateSet(workout, 0, 3), null);
  assert.strictEqual(protocol.locateSet(workout, 5, 0), null);
});

test('buildCompleted sends reps, weight and RPE from the watch', function () {
  var found = protocol.locateSet(workout, 0, 2);
  assert.deepStrictEqual(protocol.buildCompleted(found.entry, found.set, 7, 95), {
    reps: 7,
    weight: '100kg',
    rpe: 9.5
  });
});

test('buildCompleted falls back to the target RPE when the set logs RPE', function () {
  var found = protocol.locateSet(workout, 0, 2);
  assert.strictEqual(protocol.buildCompleted(found.entry, found.set, 5, -1).rpe, 9);
});

test('buildCompleted fills both sides of a unilateral timed set and prompted vars', function () {
  var found = protocol.locateSet(workout, 1, 0);
  assert.deepStrictEqual(protocol.buildCompleted(found.entry, found.set, 1, -1), {
    reps: 1,
    repsLeft: 1,
    setTimer: 30,
    setTimerLeft: 30,
    userVars: { target: '30s', rpe: 8 }
  });
});

test('buildCompleted refuses a set that asks for a weight it does not have', function () {
  var set = Object.assign({}, workout.entries[1].sets[0], { askWeight: true });
  assert.throws(function () {
    protocol.buildCompleted(workout.entries[1], set, 5, -1);
  }, /needs a weight/);
});
