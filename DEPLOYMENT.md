# Deployment Package for Azure Static Web App

This folder contains the deployment-ready files for your Static Web App.

## Structure:
- `cloud-frontend/` → Frontend (will be deployed to root)
- `api/` → Backend Azure Functions (integrated API)
- `staticwebapp.config.json` → Static Web App configuration

## Deploy via Azure Portal:

1. Go to https://portal.azure.com
2. Navigate to your Static Web App: **linus-motor**
3. Click **"Upload"** or **"Manage deployment token"**
4. Use the deployment token: `1b227bcac2ada4cba3255ff37e760e278dd7d5a807ed47fbe18e63b1b7373fa106-200c399f-999a-4007-b6ec-5e09ae51d1e900317220a48e7503`

## OR Deploy via SWA CLI (if you have Node.js):

```bash
npm install -g @azure/static-web-apps-cli
swa deploy ./cloud-frontend --api-location ./api --deployment-token "YOUR_TOKEN"
```

## OR Set up GitHub Actions:

See `.github/workflows/azure-static-web-apps.yml` for automated deployment on every git push.

## Configuration

Your API settings are already configured:
- `API_KEY`: e70652bed1a4062be80e0c7c0c6a11d517ab4ab227f79baa0994ac8f33725613
- Storage connection string: Configured ✓
- Queue name: webmotor-commands ✓
- Table name: webmotorstate ✓

## After Deployment

Your application will be available at:
- **Frontend**: https://calm-river-0a48e7503.6.azurestaticapps.net
- **API**: https://calm-river-0a48e7503.6.azurestaticapps.net/api/*

Test the health endpoint:
```bash
curl https://calm-river-0a48e7503.6.azurestaticapps.net/api/health
```
