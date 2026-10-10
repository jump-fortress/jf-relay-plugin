export function encodeSnapshot(value, server) {
  if (!value || value.type !== 'relay_progress' || value.server_code !== server) return null;
  if (typeof value.map !== 'string' || !/^[A-Za-z0-9_-]{1,128}$/.test(value.map)
      || !Array.isArray(value.userids) || value.userids.length > 100
      || value.userids.some(id => !Number.isInteger(id) || id <= 0 || id > 2147483647)
      || new Set(value.userids).size !== value.userids.length) {
    throw new Error('Invalid progress snapshot');
  }

  return `JF1 ${value.map}${value.userids.length ? ' ' + value.userids.join(' ') : ''}\n`;
}

export const instances = [...'ABCDEF'];

/** @param {import('./messages.js').SpectatorSelect} value */
export function selections(value, snapshot, server) {
  if (value?.type !== 'spectator_select' || value.server_code !== server) return [];

  if (!snapshot || !Array.isArray(snapshot.players)) throw new Error('Progress feed unavailable');

  return instances.flatMap(instance => {
    const account = value[`player${instance}`];
    if (account === undefined) return [];

    if (!Number.isInteger(account) || account <= 0 || account > 2147483647) {
      throw new Error(`Invalid player${instance} account`);
    }

    const matches = snapshot.players.filter(player => player.account === account);
    const userid = matches.length === 1 ? matches[0].userid : null;

    if (!Number.isInteger(userid) || !snapshot.userids.includes(userid)) {
      return [{ instance, error: `Account ${account} is not an active runner` }];
    }

    return [{ instance, line: `SELECT ${userid}\n` }];
  });
}
