import { test } from 'node:test';
import assert from 'node:assert/strict';
import { encodeSnapshot, selections } from './protocol.mjs';

test('snapshot preserves rank order and filters publishers', () => {
  const snapshot = { type: 'relay_progress', server_code: 'NA-LA2', map: 'jump_rush', userids: [456, 123] };
  assert.equal(encodeSnapshot(snapshot, 'NA-LA2'), 'JF1 jump_rush 456 123\n');
  assert.equal(encodeSnapshot(snapshot, 'OTHER'), null);
  assert.equal(encodeSnapshot({ ...snapshot, userids: [] }, 'NA-LA2'), 'JF1 jump_rush\n');
});

test('account selections route independently to A through F', () => {
  const snapshot = { userids: [42, 17], players: [{ userid: 42, account: 123 }, { userid: 17, account: 456 }] };
  const value = { type: 'spectator_select', server_code: 'LA', playerA: 456, playerF: 123 };
  assert.deepEqual(selections(value, snapshot, 'LA'), [
    { instance: 'A', line: 'SELECT 17\n' },
    { instance: 'F', line: 'SELECT 42\n' },
  ]);
  assert.deepEqual(selections(value, snapshot, 'OTHER'), []);
  assert.throws(() => selections(value, undefined, 'LA'));
  assert.throws(() => selections({ ...value, playerA: '123;quit' }, snapshot, 'LA'));
  assert.equal(selections({ ...value, playerA: 999 }, snapshot, 'LA')[0].error, 'Account 999 is not an active runner');
});

test('malformed IDs and command injection are rejected', () => {
  const base = { type: 'relay_progress', server_code: 'LA', map: 'jump_rush', userids: [1] };
  for (const userids of [[0], [-1], [1, 1], ['1;quit'], [2147483648]]) {
    assert.throws(() => encodeSnapshot({ ...base, userids }, 'LA'));
  }
  assert.throws(() => encodeSnapshot({ ...base, map: 'jump_rush\nquit' }, 'LA'));
});
