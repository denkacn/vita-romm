# RomM OpenAPI Snapshot

`romm-openapi-demo.json` is a saved OpenAPI schema for RomM API `5.1.0`.

It is useful as a local reference when checking RomM endpoints, request parameters, response fields, and schemas. The Vita app does not read this file at runtime, and it is not required on the PS Vita.

The file was moved into `Doc` because it is documentation/reference material, not an app asset.

Before publishing a repository, it is still worth checking it with:

```powershell
rg -n "rmm_|api_token|server_url|your-private-host" Doc
```

As of this cleanup, it appears to be an API schema only, not a RomM export containing your personal game library or token.
