const terminal = document.querySelector("#terminal");
const connectionStatus = document.querySelector("#connection-status");

if (!(terminal instanceof HTMLElement)) {
   throw new Error("Terminal element is missing");
}

if (!(connectionStatus instanceof HTMLElement)) {
   throw new Error("Connection-status element is missing");
}

const websocketUrl = new URL("/terminal", window.location.href);
websocketUrl.protocol = "wss:";

const socket = new WebSocket(websocketUrl);

socket.addEventListener("open", () => {
   connectionStatus.textContent = "Connected";
   terminal.focus();
});

socket.addEventListener("message", (event) => {
   if (typeof event.data !== "string") {
      return;
   }

   // For the echo test, append received text directly.
   // The VT parser will replace this once terminal handling begins.
   terminal.textContent += event.data;
   terminal.scrollTop = terminal.scrollHeight;
});

socket.addEventListener("close", () => {
   connectionStatus.textContent = "Disconnected";
});

socket.addEventListener("error", () => {
   connectionStatus.textContent = "Connection error";
});

terminal.addEventListener("keydown", (event) => {
   if (socket.readyState !== WebSocket.OPEN) {
      return;
   }

   const encodedKey = encodeKey(event);

   if (encodedKey === null) {
      return;
   }

   event.preventDefault();
   socket.send(encodedKey);
});

function encodeKey(event) {
   if (event.metaKey || event.ctrlKey || event.altKey) {
      return null;
   }

   switch (event.key) {
   case "Enter":
      return "\r\n";

   case "Backspace":
      return "\b";

   case "Tab":
      return "\t";

   default:
      return event.key.length === 1 ? event.key : null;
   }
}

