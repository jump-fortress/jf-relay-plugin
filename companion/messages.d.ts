export type Instance = 'A' | 'B' | 'C' | 'D' | 'E' | 'F';

export interface RelayProgress {
  type: 'relay_progress';
  protocol: 1;
  server_code: string;
  map: string;
  userids: number[];
  players: { userid: number; account: number; attempt: number; level: number }[];
}

export type SpectatorSelect = {
  type: 'spectator_select';
  server_code: string;
} & Partial<Record<`player${Instance}`, number>>;

export type FeedMessage = RelayProgress | SpectatorSelect;
