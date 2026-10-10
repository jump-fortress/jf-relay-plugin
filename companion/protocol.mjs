export function encodeSnapshot(value, server) {
  if (!value || value.type !== 'relay_progress' || value.server_code !== server) return null;
  if (typeof value.map !== 'string' || !/^[A-Za-z0-9_-]{1,128}$/.test(value.map)
      || !Array.isArray(value.userids) || value.userids.length > 100
      || value.userids.some(id => !Number.isInteger(id) || id <= 0 || id > 2147483647)
      || new Set(value.userids).size !== value.userids.length) {
    throw new Error('Invalid progress snapshot');
  }

  if (!Array.isArray(value.players) || value.players.length > 100
      || value.players.some(player => !player || typeof player.running !== 'boolean')) {
    throw new Error('Invalid selectable players');
  }

  const selectable = value.players.map(player => player.userid);
  const running = value.players.filter(player => player.running).map(player => player.userid);
  if (selectable.length > 100
      || selectable.some(id => !Number.isInteger(id) || id <= 0 || id > 2147483647)
      || new Set(selectable).size !== selectable.length
      || running.length !== value.userids.length
      || running.some((id, index) => id !== value.userids[index])) {
    throw new Error('Invalid selectable players');
  }

  return `JF2 ${value.map} ${value.userids.join(' ')} | ${selectable.join(' ')}\n`;
}

export const instances = [...'ABCDEF'];

/** @param {import('./messages.js').JFRelaySpectatorSelectEvent} value */
export function selections(value, snapshot) {
  if (value?.type !== 'spectator_select') return [];

  if (!value.value || typeof value.value !== 'object' || Array.isArray(value.value)) {
    throw new Error('Invalid spectator selection value');
  }

  if (!snapshot || !Array.isArray(snapshot.players)) throw new Error('Progress feed unavailable');

  return instances.flatMap(instance => {
    const account = value.value[`player${instance}`];
    if (account === undefined || account === null) return [];

    if (!Number.isInteger(account) || account <= 0 || account > 2147483647) {
      throw new Error(`Invalid player${instance} account`);
    }

    const matches = snapshot.players.filter(player => player.account === account);
    const userid = matches.length === 1 ? matches[0].userid : null;

    if (!Number.isInteger(userid) || userid <= 0 || userid > 2147483647) {
      return [{ instance, error: `Account ${account} is not a selectable relay bot` }];
    }

    return [{ instance, line: `SELECT ${userid}\n` }];
  });
}
