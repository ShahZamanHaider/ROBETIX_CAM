import fs from 'node:fs';
// Same public source generates the ESP32 service configuration.
const origins=JSON.parse(fs.readFileSync(new URL('../service-origins.json',import.meta.url),'utf8'));
export const customerOrigin=origins.customer;
export function configuredAuthOrigin(req,requestOrigin){
 const override=process.env.AUTH_APP_ORIGIN;
 return override||(process.env.VERCEL_ENV==='preview'||process.env.NODE_ENV==='test'?requestOrigin(req):customerOrigin);
}
