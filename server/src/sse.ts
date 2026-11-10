import { Response } from 'express';

interface Client {
  id: number;
  res: Response;
}

let clients: Client[] = [];
let clientId = 0;

setInterval(() => {
  const payload = ': ping\n\n';
  const currentClients = [...clients];
  for (const client of currentClients) {
    try {
      client.res.write(payload);
      (client.res as any).flush?.();
    } catch {
      removeSSEClient(client.id);
    }
  }
}, 15000);

export function addSSEClient(res: Response): number {
  const id = ++clientId;
  clients.push({ id, res });
  return id;
}

export function removeSSEClient(id: number) {
  clients = clients.filter(c => c.id !== id);
}

export function broadcastEvent(event: string, data: any) {
  const payload = `event: ${event}\ndata: ${JSON.stringify(data)}\n\n`;
  const currentClients = [...clients];
  for (const client of currentClients) {
    try {
      client.res.write(payload);
      (client.res as any).flush?.();
    } catch {
      removeSSEClient(client.id);
    }
  }
}
