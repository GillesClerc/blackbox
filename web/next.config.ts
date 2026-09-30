import type { NextConfig } from "next";

const isDev = process.env.NODE_ENV === "development";

// CSP sans nonce (guide Next 16 « Content Security Policy », section Without
// Nonces) : le navigateur ne parle qu'à notre propre origine (server actions,
// /api/box/*) ; les polices next/font sont auto-hébergées. À élargir quand
// Stripe arrivera (js.stripe.com, api.stripe.com).
const csp = [
  "default-src 'self'",
  `script-src 'self' 'unsafe-inline'${isDev ? " 'unsafe-eval'" : ""}`,
  "style-src 'self' 'unsafe-inline'",
  "img-src 'self' blob: data:",
  "font-src 'self'",
  "connect-src 'self'",
  "object-src 'none'",
  "base-uri 'self'",
  "form-action 'self'",
  "frame-ancestors 'none'",
  ...(isDev ? [] : ["upgrade-insecure-requests"]),
].join("; ");

const securityHeaders = [
  { key: "Content-Security-Policy", value: csp },
  { key: "Strict-Transport-Security", value: "max-age=31536000; includeSubDomains" },
  { key: "X-Content-Type-Options", value: "nosniff" },
  { key: "X-Frame-Options", value: "DENY" },
  { key: "Referrer-Policy", value: "strict-origin-when-cross-origin" },
  // bluetooth=(self) : /devices/add appaire la box en Web Bluetooth.
  {
    key: "Permissions-Policy",
    value: "bluetooth=(self), camera=(), microphone=(), geolocation=(), payment=()",
  },
];

const nextConfig: NextConfig = {
  async headers() {
    return [{ source: "/(.*)", headers: securityHeaders }];
  },
};

export default nextConfig;
