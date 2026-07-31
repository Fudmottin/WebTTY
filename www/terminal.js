class TerminalScreen {
   constructor() {
      this.lines = [""];
      this.row = 0;
      this.column = 0;
   }

   write(character) {
      const line = this.lines[this.row];
      const paddingLength = Math.max(0, this.column - line.length);
      const paddedLine = line + " ".repeat(paddingLength);

      this.lines[this.row] =
         paddedLine.slice(0, this.column) +
         character +
         paddedLine.slice(this.column + 1);

      ++this.column;
   }

   carriageReturn() {
      this.column = 0;
   }

   lineFeed() {
      ++this.row;

      if (this.row === this.lines.length) {
         this.lines.push("");
      }
   }

   backspace() {
      if (this.column > 0) {
         --this.column;
      }
   }

   horizontalTab() {
      const tabWidth = 8;
      this.column += tabWidth - (this.column % tabWidth);
   }

   eraseToEndOfLine() {
      const line = this.lines[this.row];

      if (this.column < line.length) {
         this.lines[this.row] = line.slice(0, this.column);
      }
   }

   moveCursorBackward(count) {
      this.column = Math.max(0, this.column - count);
   }

   moveCursorForward(count) {
      this.column += count;
   }

   text() {
      return this.lines.join("\n");
   }
}

class VtParser {
   constructor(screen) {
      this.screen = screen;
      this.state = "ground";
      this.csiParameters = "";
   }

   write(text) {
      // Parser state survives WebSocket messages because escape sequences
      // may be divided across transport boundaries.
      for (const character of text) {
         this.consume(character);
      }
   }

   consume(character) {
      switch (this.state) {
      case "ground":
         this.consumeGround(character);
         break;

      case "escape":
         this.consumeEscape(character);
         break;

      case "csi":
         this.consumeCsi(character);
         break;

      case "osc":
         this.consumeOsc(character);
         break;

      case "osc-escape":
         this.consumeOscEscape(character);
         break;

      default:
         throw new Error(`Unknown VT parser state: ${this.state}`);
      }
   }

   consumeGround(character) {
      switch (character) {
      case "\x1b":
         this.state = "escape";
         break;

      case "\r":
         this.screen.carriageReturn();
         break;

      case "\n":
         this.screen.lineFeed();
         break;

      case "\b":
         this.screen.backspace();
         break;

      case "\t":
         this.screen.horizontalTab();
         break;

      default:
         if (isPrintable(character)) {
            this.screen.write(character);
         }
         break;
      }
   }

   consumeEscape(character) {
      switch (character) {
      case "[":
         this.csiParameters = "";
         this.state = "csi";
         break;

      case "]":
         this.state = "osc";
         break;

      case "\x1b":
         // A new ESC restarts escape-sequence recognition.
         break;

      default:
         // Unsupported short escape sequences are consumed as a unit.
         this.state = "ground";
         break;
      }
   }

   consumeCsi(character) {
      if (character === "\x1b") {
         this.state = "escape";
         return;
      }

      if (isCsiParameterByte(character)) {
         this.csiParameters += character;
         return;
      }

      if (isCsiFinalByte(character)) {
         this.dispatchCsi(character);
         this.state = "ground";
      }
   }

   consumeOsc(character) {
      switch (character) {
      case "\x07":
         // OSC may be terminated by BEL.
         this.state = "ground";
         break;

      case "\x1b":
         // OSC may also end with the two-character ST sequence: ESC \.
         this.state = "osc-escape";
         break;

      default:
         break;
      }
   }

   consumeOscEscape(character) {
      if (character === "\\") {
         this.state = "ground";
         return;
      }

      // ESC not followed by '\' was part of the ignored OSC payload.
      this.state = character === "\x1b" ? "osc-escape" : "osc";
   }

   dispatchCsi(finalByte) {
      if (
         finalByte === "K" &&
         (this.csiParameters === "" || this.csiParameters === "0")
      ) {
         this.screen.eraseToEndOfLine();
         return;
      }

      if (finalByte === "D") {
         const count = parseCsiCount(this.csiParameters);

         if (count !== null) {
            this.screen.moveCursorBackward(count);
         }
      }

      if (finalByte === "C") {
         const count = parseCsiCount(this.csiParameters);

         if (count !== null) {
            this.screen.moveCursorForward(count);
         }
      }

      // Unsupported CSI commands are intentionally consumed without effect.
   }
}

class TerminalView {
   constructor(element) {
      if (!(element instanceof HTMLElement)) {
         throw new Error("Terminal element is missing");
      }

      this.element = element;
      this.screen = new TerminalScreen();
      this.parser = new VtParser(this.screen);
   }

   write(text) {
      this.parser.write(text);
      this.render();
   }

   render() {
      this.element.textContent = this.screen.text();
      this.element.scrollTop = this.element.scrollHeight;
   }

   focus() {
      this.element.focus();
   }
}

function isPrintable(character) {
   const codePoint = character.codePointAt(0);

   if (codePoint === undefined) {
      return false;
   }

   return codePoint >= 0x20 && codePoint !== 0x7f;
}

function isCsiFinalByte(character) {
   const codePoint = character.codePointAt(0);

   return codePoint !== undefined &&
      codePoint >= 0x40 &&
      codePoint <= 0x7e;
}

function isCsiParameterByte(character) {
   const codePoint = character.codePointAt(0);

   return codePoint !== undefined &&
      codePoint >= 0x30 &&
      codePoint <= 0x3f;
}

function parseCsiCount(parameters) {
   if (parameters === "") {
      return 1;
   }

   if (!/^[0-9]+$/.test(parameters)) {
      return null;
   }

   const count = Number.parseInt(parameters, 10);

   return count === 0 ? 1 : count;
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

terminalElement.addEventListener("paste", (event) => {
   if (socket.readyState !== WebSocket.OPEN) {
      return;
   }

   const text = event.clipboardData?.getData("text/plain");

   if (text === undefined || text.length === 0) {
      return;
   }

   // The browser must not insert pasted text into the DOM. The terminal
   // display is driven solely by bytes echoed back from the PTY.
   event.preventDefault();

   socket.send(text);
});

function encodeKey(event) {
   if (event.metaKey || event.ctrlKey || event.altKey) {
      return null;
   }

   switch (event.key) {
   case "Enter":
      return "\r";

   case "Backspace":
      return "\b";

   case "Tab":
      return "\t";

   default:
      return event.key.length === 1 ? event.key : null;
   }
}

