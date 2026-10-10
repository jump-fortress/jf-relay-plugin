export type Instance = 'A' | 'B' | 'C' | 'D' | 'E' | 'F';

export interface RelayProgress {
  type: 'relay_progress';
  protocol: 1;
  server_code: string;
  map: string;
  userids: number[];
  players: { userid: number; account: number; attempt: number; level: number }[];
}

export interface JFRelayEvent {
  type: string;
  server_code?: string;
}

export interface JFRelaySpectatorSelectEvent extends JFRelayEvent {
  type: 'spectator_select';
  value: {
    playerA: number | null;
    playerB: number | null;
  } & Partial<Record<`player${Exclude<Instance, 'A' | 'B'>}`, number | null>>;
}

export type FeedMessage = RelayProgress | JFRelaySpectatorSelectEvent;
