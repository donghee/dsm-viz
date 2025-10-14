function calculateChiSquare(data) {
  if (!data || data.length === 0) return 0;

  // Convert string to bytes if needed
  const bytes = typeof data === 'string' ?
    new TextEncoder().encode(data) : data;

  if (bytes.length === 0) return 0;

  // Count frequency of each byte value (0-255)
  const observed = new Array(256).fill(0);
  for (let i = 0; i < bytes.length; i++) {
    observed[bytes[i]]++;
  }

  // Expected frequency for uniform distribution
  const expected = bytes.length / 256;

  // Calculate chi-square statistic
  let chiSquare = 0.0;
  for (let i = 0; i < 256; i++) {
    if (expected > 0) {
      const diff = observed[i] - expected;
      chiSquare += (diff * diff) / expected;
    }
  }

  return chiSquare;

}

function testChiSquare2(data) {
  console.log("Testing Chi-Square Analysis");
  console.log("=========================");


  try {
    console.log("Input data:");
    console.log(data);
    console.log();

    // Convert hex to bytes
    const dataBytes = hexStringToBytes(data);
    console.log(`Converted to ${dataBytes.length} bytes`);

    // Calculate chi-square
    const chiSquare = calculateChiSquare(dataBytes);
    console.log(`Chi-Square value: ${chiSquare.toFixed(6)}`);

    // Show byte statistics
    const observed = new Array(256).fill(0);
    for (let i = 0; i < dataBytes.length; i++) {
      observed[dataBytes[i]]++;
    }

    const uniqueBytes = observed.filter(count => count > 0).length;
    const totalBytes = dataBytes.length;
    const expectedFreq = totalBytes / 256;

    console.log(`\nStatistics:`);
    console.log(`- Total bytes: ${totalBytes}`);
    console.log(`- Unique byte values: ${uniqueBytes}/256`);
    console.log(`- Expected frequency per byte: ${expectedFreq.toFixed(4)}`);

    // Show all byte frequencies
    console.log(`\nByte frequencies:`);
    const nonZeroFreq = observed
      .map((count, value) => ({ value, count }))
      .filter(item => item.count > 0)
      .sort((a, b) => a.value - b.value);

    nonZeroFreq.forEach(item => {
      const hex = item.value.toString(16).padStart(2, '0').toUpperCase();
      const deviation = (item.count - expectedFreq).toFixed(4);
      console.log(`  0x${hex} (${item.value.toString().padStart(3)}): ${item.count} times (deviation: ${deviation})`);
    });

    // Interpretation
    console.log(`\nInterpretation:`);
    if (chiSquare < 200) {
      console.log("- Relatively uniform distribution (lower chi-square)");
    } else if (chiSquare < 500) {
      console.log("- Moderate non-uniformity");
    } else {
      console.log("- High non-uniformity (structured data pattern)");
    }

    return {
      data: data,
      byteCount: data.length,
      chiSquare: chiSquare,
      uniqueBytes: uniqueBytes,
      frequencies: nonZeroFreq
    };

  } catch (error) {
    console.error(`Error: ${error.message}`);
    return null;
  }
}

// Export functions for use in other modules
if (typeof module !== 'undefined' && module.exports) {
  module.exports = {
    calculateChiSquare,
    generateRandomData,
    hexStringToBytes,
    testChiSquare,
    testChiSquare2,
  };
}

// Generate random data for testing
function generateRandomData(length) {
  const data = new Uint8Array(length);
  for (let i = 0; i < length; i++) {
    data[i] = Math.floor(Math.random() * 256);
  }
  return data;
}

// Convert hex string to byte array
function hexStringToBytes(hexString) {
  // Remove spaces and convert to uppercase
  const cleanHex = hexString.replace(/\s+/g, '').toUpperCase();

  // Validate hex string
  if (!/^[0-9A-F]*$/.test(cleanHex)) {
    throw new Error('Invalid hex string: contains non-hexadecimal characters');
  }

  if (cleanHex.length % 2 !== 0) {
    throw new Error('Invalid hex string: odd length');
  }

  const bytes = new Uint8Array(cleanHex.length / 2);
  for (let i = 0; i < cleanHex.length; i += 2) {
    bytes[i / 2] = parseInt(cleanHex.substr(i, 2), 16);
  }

  return bytes;
}

// Test function to compare English text vs random data
function testChiSquare() {
  console.log("Chi-Square Test Results");
  console.log("======================");

  // English text samples
  const englishTexts = [
    "The quick brown fox jumps over the lazy dog. This sentence contains every letter of the alphabet at least once.",
    "To be or not to be, that is the question. Whether 'tis nobler in the mind to suffer the slings and arrows of outrageous fortune.",
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.",
    "Hello world! This is a simple test message to analyze the distribution of characters in English text.",
    "JavaScript is a versatile programming language that can be used for both frontend and backend development."
  ];

  console.log("\n--- English Text Analysis ---");
  englishTexts.forEach((text, index) => {
    const chiSquare = calculateChiSquare(text);
    console.log(`Text ${index + 1} (${text.length} chars): Chi-Square = ${chiSquare.toFixed(2)}`);
    console.log(`Sample: "${text.substring(0, 50)}..."`);
  });

  console.log("\n--- Random Data Analysis ---");
  const randomSizes = [100, 500, 1000, 2000, 5000];

  randomSizes.forEach(size => {
    const randomData = generateRandomData(size);
    const chiSquare = calculateChiSquare(randomData);
    console.log(`Random data (${size} bytes): Chi-Square = ${chiSquare.toFixed(2)}`);
  });

  console.log("\n--- Hex Data Analysis ---");

  // Test with the provided hex data
  const hexData = "01 5c 00 38 fd 2c 00 00 7b 01 01 18 00 00 40 c4 b6 a3 00 00 00 00 9c 2d af 15 77 5b ea 4b 4a 71 00 00 46 00 6e 00 06 00 5f 70 03 0a 4a 71 00 00 84 03 00 00 f4 06 00 00 90 01 c7 5e";

  try {
    const hexBytes = hexStringToBytes(hexData);
    const hexChiSquare = calculateChiSquare(hexBytes);

    console.log(`Hex data (${hexBytes.length} bytes): Chi-Square = ${hexChiSquare.toFixed(2)}`);
    console.log(`Hex string: "${hexData.substring(0, 50)}..."`);

    // Show byte distribution summary
    const observed = new Array(256).fill(0);
    for (let i = 0; i < hexBytes.length; i++) {
      observed[hexBytes[i]]++;
    }
    const uniqueBytes = observed.filter(count => count > 0).length;
    console.log(`Unique byte values: ${uniqueBytes}/256`);

    // Show most frequent bytes
    const sortedFreq = observed
      .map((count, value) => ({ value, count }))
      .filter(item => item.count > 0)
      .sort((a, b) => b.count - a.count)
      .slice(0, 5);

    console.log("Most frequent bytes:");
    sortedFreq.forEach(item => {
      console.log(`  0x${item.value.toString(16).padStart(2, '0').toUpperCase()}: ${item.count} times`);
    });

  } catch (error) {
    console.error(`Error processing hex data: ${error.message}`);
  }

  console.log("\n--- Comparison Analysis ---");

  // Compare same-length data
  const testLength = 1000;
  const longEnglishText = englishTexts.join(" ").repeat(Math.ceil(testLength / englishTexts.join(" ").length)).substring(0, testLength);
  const randomData = generateRandomData(testLength);

  const englishChiSquare = calculateChiSquare(longEnglishText);
  const randomChiSquare = calculateChiSquare(randomData);

  console.log(`\nSame length comparison (${testLength} bytes):`);
  console.log(`English text Chi-Square: ${englishChiSquare.toFixed(2)}`);
  console.log(`Random data Chi-Square: ${randomChiSquare.toFixed(2)}`);
  console.log(`Difference: ${Math.abs(englishChiSquare - randomChiSquare).toFixed(2)}`);

  // Analysis interpretation
  console.log("\n--- Interpretation ---");
  console.log("Lower Chi-Square values indicate more uniform byte distribution");
  console.log("Higher Chi-Square values indicate less uniform (more structured) data");
  console.log("English text typically shows higher values due to character frequency patterns");
  console.log("Random data should show lower values due to more uniform distribution");

  return {
    englishResults: englishTexts.map(text => ({
      text: text.substring(0, 50) + "...",
      length: text.length,
      chiSquare: calculateChiSquare(text)
    })),
    randomResults: randomSizes.map(size => ({
      size: size,
      chiSquare: calculateChiSquare(generateRandomData(size))
    })),
    comparison: {
      english: englishChiSquare,
      random: randomChiSquare,
      difference: Math.abs(englishChiSquare - randomChiSquare)
    }
  };
}

// node -e "const { testChiSquare2 } = require('./chi-square.js'); testChiSquare2('01 5c 00 38 fd 2c 00 00 7b 01 01 18 00 00 40 c4 b6 a3 00 00 00 00 9c 2d af 15 77 5b ea 4b 4a 71 00 00 46 00 6e 00 06 00 5f 70 03 0a 4a 71 00 00 84 03 00 00 f4 06 00 00 90 01 c7 5e');"

// node -e "const { testChiSquare2 } = require('./chi-square.js'); testChiSquare2('03 5d 00 5d 17 fe fd 00 01 00 00 00 00 06 75 00 50 00 01 00 00 00 00 06 75 c9 7e 81 94 92 cb 24 10 28 5c 88 45 70 48 0a bd 3a 06 06 31 14 b9 fb 98 98 b5 97 6d 91 7a b1 b8 24 77 0f 9e c4 c7 df 7c 4b 35 5a 5a 34 0d e1 19 68 4f 69 20 21 43 46 22 b5 5a 41 bd 98 e3 84 09 38 83 1d 95 d5 60 9b b2');"
