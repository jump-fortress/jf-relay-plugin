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
