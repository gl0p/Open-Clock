/*
 * This file is part of the open-clock project.
 * 
 * Copyright (C) 2024 gl0p
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

const CACHE_NAME = 'clock-control-panel-v2.04'; // Updated cache version

const urlsToCache = [
  '/time', // Ensure /time is explicitly cached
  '/manifest.json',
  '/service-worker.js',
  'https://cdn.jsdelivr.net/npm/@jaames/iro@5', // iro.js library
  'https://cdnjs.cloudflare.com/ajax/libs/noUiSlider/15.7.0/nouislider.min.css',
  'https://cdnjs.cloudflare.com/ajax/libs/noUiSlider/15.7.0/nouislider.min.js',
];

// Install event: Cache all necessary files
self.addEventListener('install', (event) => {
  console.log('Service Worker: Installing...');
  event.waitUntil(
    caches.open(CACHE_NAME)
      .then((cache) => {
        console.log('Opened cache');
        return cache.addAll(urlsToCache);
      })
      .then(() => self.skipWaiting()) // Activate immediately
      .catch((err) => console.error('Cache install failed:', err))
  );
});

// Fetch event: Serve cached content when possible
self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url);

  // Bypass caching for dynamic API requests (e.g., /get or /set)
  if (url.pathname.startsWith('/get') || url.pathname.startsWith('/set')) {
    event.respondWith(fetch(event.request));
    return;
  }

  // Special handling for /time route
  if (url.pathname === '/time') {
      event.respondWith(
        caches.open(CACHE_NAME).then(async (cache) => {
          const cachedResponse = await cache.match('/time');
          if (cachedResponse) {
            return cachedResponse;
          }
          const networkResponse = await fetch(event.request);
          if (networkResponse && networkResponse.ok) {
            cache.put('/time', networkResponse.clone());
          }
          return networkResponse;
        }).catch((error) => {
          console.error('Failed to fetch /time:', error);
          return new Response('Time page data unavailable', { status: 503 });
        })
      );
      return;
    }


  // Default strategy for all other requests (CSS, JS, etc.)
  event.respondWith(
    caches.match(event.request).then((cachedResponse) => {
      return cachedResponse || fetch(event.request).then((networkResponse) => {
        return caches.open(CACHE_NAME).then((cache) => {
          cache.put(event.request, networkResponse.clone());
          return networkResponse;
        });
      });
    })
  );
});

// Activate event: Clean up old caches
self.addEventListener('activate', (event) => {
  console.log('Service Worker: Activating...');
  const cacheWhitelist = [CACHE_NAME]; // Keep only the current cache
  event.waitUntil(
    caches.keys().then((cacheNames) =>
      Promise.all(
        cacheNames.map((cacheName) => {
          if (!cacheWhitelist.includes(cacheName)) {
            console.log('Deleting old cache:', cacheName);
            return caches.delete(cacheName);
          }
        })
      )
    ).then(() => self.clients.claim()) // Take control of all pages immediately
  );
});
