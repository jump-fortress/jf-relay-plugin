import { test } from 'node:test';
import assert from 'node:assert/strict';
import { encodeSnapshot } from './protocol.mjs';

test('snapshot preserves rank order and filters publishers', () => {
  const snapshot = { type: 'relay_progress', server_code: 'NA-LA2', map: 'jump_rush', userids: [456, 123] };
  assert.equal(encodeSnapshot(snapshot, 'NA-LA2'), 'JF1 jump_rush 456 123\n');
  assert.equal(encodeSnapshot(snapshot, 'OTHER'), null);
  assert.equal(encodeSnapshot({ ...snapshot, userids: [] }, 'NA-LA2'), 'JF1 jump_rush\n');
});

test('malformed IDs and command injection are rejected', () => {
  const base = { type: 'relay_progress', server_code: 'LA', map: 'jump_rush', userids: [1] };
  for (const userids of [[0], [-1], [1, 1], ['1;quit'], [2147483648]]) {
    assert.throws(() => encodeSnapshot({ ...base, userids }, 'LA'));
  }
  assert.throws(() => encodeSnapshot({ ...base, map: 'jump_rush\nquit' }, 'LA'));
});
