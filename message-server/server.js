const express = require("express");
const cors = require("cors");

const app = express();
app.use(cors());
app.use(express.json());

let latestMessage = "no message yet";
let lastUpdateTimestamp = 0;
let lastSenderId = ""; // Stores the ID of the device that sent the message

app.get("/", (req, res) => {
  res.json({ status: "server running" });
});

app.post("/message", (req, res) => {
  latestMessage = req.body.message;
  lastSenderId = req.body.id; // Save the sender's unique ID
  lastUpdateTimestamp = Date.now();
  
  console.log(`Received from [${lastSenderId}]:`, latestMessage);

  res.json({
    success: true,
    stored: latestMessage,
    sender_id: lastSenderId
  });
});

app.get("/message", (req, res) => {
  const now = Date.now();
  const secondsAgo = lastUpdateTimestamp === 0 ? 999 : Math.floor((now - lastUpdateTimestamp) / 1000);

  res.json({
    message: latestMessage,
    seconds_ago: secondsAgo,
    sender_id: lastSenderId // Tell the requester who sent this
  });
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`Server running on port ${PORT}`);
});