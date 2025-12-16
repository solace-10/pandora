/**
 * Welcome to Cloudflare Workers! This is your first worker.
 *
 * - Run `npm run dev` in your terminal to start a development server
 * - Open a browser tab at http://localhost:8787/ to see your worker in action
 * - Run `npm run deploy` to publish your worker
 *
 * Bind resources to your worker in `wrangler.jsonc`. After adding bindings, a type definition for the
 * `Env` object can be regenerated with `npm run cf-typegen`.
 *
 * Learn more at https://developers.cloudflare.com/workers/
 */

import { WorkerEntrypoint } from "cloudflare:workers";

// Check requests for a pre-shared secret
const hasValidHeader = (request, env) => {
  return request.headers.get("X-Custom-Auth-Key") === env.FORGE_AUTH_KEY_SECRET;
};

function authorizeRequest(request, env, key) {
  switch (request.method) {
    case "PUT":
      return hasValidHeader(request, env);
    case "HEAD":
    case "GET":
      return true;
    default:
      return false;
  }
}

export default class extends WorkerEntrypoint<Env> {
  async fetch(request: Request) {
    const url = new URL(request.url);
    const key = url.pathname.slice(1);

    if (!authorizeRequest(request, this.env, key)) {
      return new Response("Forbidden", { status: 403 });
    }
    
    switch (request.method) {
      case "PUT": {
        await this.env.PANDORA_WEB_BUCKET.put(key, request.body, {
          onlyIf: request.headers,
          httpMetadata: request.headers,
        });
        return new Response(`OK`);
      }
      case "GET": {
        const object = await this.env.PANDORA_WEB_BUCKET.get(key, {
          onlyIf: request.headers,
          range: request.headers,
        });

        if (object === null) {
          return new Response("Object Not Found", { status: 404 });
        }

        const headers = new Headers();
        object.writeHttpMetadata(headers);
        headers.set("etag", object.httpEtag);

        // When no body is present, preconditions have failed
        return new Response("body" in object ? object.body : undefined, {
          status: "body" in object ? 200 : 412,
          headers,
        });
      }
      case "HEAD": {
        const object = await this.env.PANDORA_WEB_BUCKET.head(key);

        if (object === null) {
          return new Response(null, { status: 404 });
        }

        const headers = new Headers();
        object.writeHttpMetadata(headers);
        headers.set("etag", object.httpEtag);
        headers.set("content-length", object.size.toString());

        return new Response(null, { status: 200, headers });
      }      
      default:
        return new Response("Method Not Allowed", {
          status: 405,
          headers: {
            Allow: "PUT, GET, HEAD",
          },
        });
    }
  }
};
