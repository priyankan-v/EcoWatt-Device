const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const UPLOAD_PSK = process.env.UPLOAD_PSK || "ColdPlay@EcoWatt2025";
const NONCE_FILE_PATH = path.join(__dirname, 'nonce.json');

function encodeBase64(payload) {
    return Buffer.from(payload, 'utf8').toString('base64');
}

function decodeBase64(encodedPayload) {
    return Buffer.from(encodedPayload, 'base64').toString('utf8');
}

function generateMAC(payload, secretKey) {
    const hmac = crypto.createHmac('sha256', secretKey);
    hmac.update(payload);
    return hmac.digest('hex');
}

function verifyMAC(macA, macB) {
    if (typeof macA !== 'string' || typeof macB !== 'string') {
        return false;
    }

    if (macA.length !== macB.length) {
        return false;
    }

    let diff = 0;
    for (let i = 0; i < macA.length; i++) {
        diff |= macA.charCodeAt(i) ^ macB.charCodeAt(i);
    }

    return diff === 0;
}

class NonceManager {
    constructor(filePath) {
        this.filePath = filePath;
        this._initializeNonceFile();
    }

    _initializeNonceFile() {
        if (!fs.existsSync(this.filePath)) {
            console.log(`Nonce file not found. Initializing with nonce = 0 at ${this.filePath}.`);
            this._writeNonce(0);
        }
    }

    _readNonce() {
        const fileContent = fs.readFileSync(this.filePath, 'utf8');
        const data = JSON.parse(fileContent);
        return parseInt(data.nonce, 10);
    }

    _writeNonce(nonce) {
        const data = JSON.stringify({ nonce });
        fs.writeFileSync(this.filePath, data, 'utf8');
    }

    verifyAndIncrementNonce(receivedNonce) {
        try {
            const expectedNonce = this._readNonce();

            // if (receivedNonce === expectedNonce || receivedNonce > expectedNonce && receivedNonce <= expectedNonce + 3) {
            if (receivedNonce === expectedNonce) {
                console.log(`Nonce verified: ${receivedNonce}`);
                this._writeNonce(receivedNonce + 1);
                return true;
            }

            console.error(`Nonce mismatch! Expected: ${expectedNonce}, Received: ${receivedNonce}`);
            return false;
        } catch (error) {
            console.error("Error processing nonce:", error);
            return false;
        }
    }

    // reset(value = 0) {
    //     console.log(`Resetting nonce to ${value}.`);
    //     this._writeNonce(value);
    // }
}

module.exports = {
    encodeBase64,
    decodeBase64,
    generateMAC,
    verifyMAC,
    NonceManager,
    NONCE_FILE_PATH,
    UPLOAD_PSK
};

