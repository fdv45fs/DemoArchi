Frontend (React) for Counter

Run locally (from frontend/):

1. Install dependencies

```sh
cd frontend
npm install
```

2. Start dev server

```sh
npm run dev
```

Open the URL printed by Vite (usually http://localhost:5173) and the app will call
the client-backend at http://localhost:8000 by default. Adjust `API_BASE` in `src/App.jsx`
or set `REACT_APP_API_BASE` environment variable when running Vite.
