import { test } from 'node:test';
import assert from 'node:assert/strict';
import { encodeSnapshot, selections } from './protocol.mjs';

test('snapshot preserves rank order and filters publishers', () => {
  const snapshot = {
    type: 'relay_progress', server_code: 'NA-LA2', map: 'jump_rush', userids: [456, 123],
    players: [{ userid: 456, running: true }, { userid: 123, running: true }],
  };
  assert.equal(encodeSnapshot(snapshot, 'NA-LA2'), 'JF2 jump_rush 456 123 | 456 123\n');
  assert.equal(encodeSnapshot(snapshot, 'OTHER'), null);
  assert.equal(encodeSnapshot({ ...snapshot, userids: [], players: [] }, 'NA-LA2'), 'JF2 jump_rush  | \n');
  assert.throws(() => encodeSnapshot({ ...snapshot, players: [{ userid: 456 }] }, 'NA-LA2'));
});

test('account selections route independently to A through F', () => {
  const snapshot = { userids: [42, 17], players: [{ userid: 42, account: 123 }, { userid: 17, account: 456 }] };
  const value = { type: 'spectator_select', value: { playerA: 456, playerB: null, playerF: 123 } };
  assert.deepEqual(selections(value, snapshot), [
    { instance: 'A', line: 'SELECT 17\n' },
    { instance: 'F', line: 'SELECT 42\n' },
  ]);
  assert.throws(() => selections(value, undefined));
  assert.throws(() => selections({ ...value, value: { playerA: '123;quit', playerB: null } }, snapshot));
  assert.equal(selections({ ...value, value: { playerA: 999, playerB: null } }, snapshot)[0].error, 'Account 999 is not a selectable relay bot');
  assert.deepEqual(selections({ type: 'spectator_select', value: { playerA: 123, playerB: null } }, snapshot), [
    { instance: 'A', line: 'SELECT 42\n' },
  ]);
  assert.deepEqual(selections({ type: 'spectator_select', value: { playerA: null, playerB: null } }, snapshot), []);
  assert.throws(() => selections({ type: 'spectator_select' }, snapshot));
});

test('overlay can select a stopped runner without adding a rank', () => {
  const snapshot = {
    type: 'relay_progress', server_code: 'LA', map: 'jump_rush', userids: [],
    players: [{ userid: 42, account: 123, running: false }],
  };
  assert.equal(encodeSnapshot(snapshot, 'LA'), 'JF2 jump_rush  | 42\n');
  assert.deepEqual(selections({ type: 'spectator_select', value: { playerA: 123 } }, snapshot), [
    { instance: 'A', line: 'SELECT 42\n' },
  ]);

  assert.throws(() => encodeSnapshot({ ...snapshot, userids: [99] }, 'LA'));
  assert.throws(() => encodeSnapshot({ ...snapshot, players: [{ userid: '42;quit' }] }, 'LA'));
});

test('malformed IDs and command injection are rejected', () => {
  const base = { type: 'relay_progress', server_code: 'LA', map: 'jump_rush', userids: [1] };
  for (const userids of [[0], [-1], [1, 1], ['1;quit'], [2147483648]]) {
    assert.throws(() => encodeSnapshot({ ...base, userids }, 'LA'));
  }
  assert.throws(() => encodeSnapshot({ ...base, map: 'jump_rush\nquit' }, 'LA'));
});
