class TerminalView {
   constructor(element) {
      if (!(element instanceof HTMLElement)) {
         throw new Error("Terminal element is missing");
      }

      this.element = element;
   }

   // Keep rendering behind this boundary so VT parsing can be added later
   // without changing the WebSocket event handling.
   write(text) {
      this.element.textContent += text;
      this.element.scrollTop = this.element.scrollHeight;
   }

   focus() {
      this.element.focus();
   }
}

const terminalElement = document.querySelector("#terminal");
const connectionStatus = document.querySelector("#connection-status");

const terminalView = new TerminalView(terminalElement);

if (!(connectionStatus instanceof HTMLElement)) {
   throw new Error("Connection-status element is missing");
}

const websocketUrl = new URL("/terminal", window.location.href);
websocketUrl.protocol = "wss:";

const socket = new WebSocket(websocketUrl);

socket.addEventListener("open", () => {
   connectionStatus.textContent = "Connected";
   terminalView.focus();
});

socket.addEventListener("message", (event) => {
   if (typeof event.data !== "string") {
      return;
   }

   // PTY output is still appended directly. A stateful VT parser will
   // eventually replace this simple rendering step.
   terminalView.write(event.data);
});

socket.addEventListener("close", () => {
   connectionStatus.textContent = "Disconnected";
});

socket.addEventListener("error", () => {
   connectionStatus.textContent = "Connection error";
});

terminalElement.addEventListener("keydown", (event) => {
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

