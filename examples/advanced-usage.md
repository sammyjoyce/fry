# Advanced Usage Examples

This guide demonstrates advanced usage patterns for the CLI application, including piping, scripting, and integration with other tools.

## Table of Contents

- [Input/Output Redirection](#inputoutput-redirection)
- [Piping and Chaining](#piping-and-chaining)
- [Scripting Examples](#scripting-examples)
- [Integration Patterns](#integration-patterns)
- [Performance Tips](#performance-tips)
- [Error Handling](#error-handling)

## Input/Output Redirection

### Reading from Files

```bash
# Process input from a file
fry process < input.txt

# Process multiple files
cat file1.txt file2.txt | fry process

# Process with specific encoding
iconv -f ISO-8859-1 -t UTF-8 input.txt | fry process
```

### Writing to Files

```bash
# Save output to a file
fry generate --format json > output.json

# Append to existing file
fry status >> log.txt

# Split output streams
fry analyze 2>errors.log 1>results.txt
```

### Silent Operation

```bash
# Suppress all output
fry batch-process > /dev/null 2>&1

# Show only errors
fry validate 1>/dev/null

# Show only progress (if using stderr for progress)
fry convert large-file.dat 2>&1 1>/dev/null | grep -E "^\[.*%\]"
```

## Piping and Chaining

### Basic Pipes

```bash
# Filter and process
grep "ERROR" log.txt | fry analyze --type error

# Chain multiple operations
fry list --format json | jq '.items[]' | fry process --batch

# Count results
fry search "pattern" | wc -l
```

### Advanced Piping

```bash
# Parallel processing with xargs
find . -name "*.dat" | xargs -P 4 -I {} fry process {}

# Process in batches
ls *.csv | xargs -n 10 fry import --batch

# Stream processing
tail -f app.log | fry monitor --real-time
```

### Complex Pipelines

```bash
# Multi-stage processing pipeline
cat raw-data.txt \
  | fry parse --format csv \
  | awk '{print $2,$4}' \
  | fry analyze --columns 2 \
  | tee results.txt \
  | fry visualize --output graph.png

# Conditional processing
fry check --quiet && fry process || echo "Check failed"

# Loop with pipe
for file in *.log; do
  echo "Processing $file"
  cat "$file" | fry parse | fry summarize >> summary.txt
done
```

## Scripting Examples

### Bash Integration

```bash
#!/bin/bash
# process-daily.sh - Daily processing script

set -euo pipefail

# Configuration
MYAPP_BIN="fry"
DATA_DIR="/var/data"
OUTPUT_DIR="/var/output"
LOG_FILE="/var/log/fry-daily.log"

# Function to process a single file
process_file() {
  local input_file="$1"
  local output_file="$2"
  
  echo "[$(date)] Processing $input_file" >> "$LOG_FILE"
  
  if $MYAPP_BIN validate "$input_file"; then
    $MYAPP_BIN process "$input_file" \
      --output "$output_file" \
      --format json \
      2>> "$LOG_FILE"
  else
    echo "[$(date)] Validation failed for $input_file" >> "$LOG_FILE"
    return 1
  fi
}

# Main processing loop
find "$DATA_DIR" -name "*.dat" -mtime -1 | while read -r file; do
  basename=$(basename "$file" .dat)
  output_file="$OUTPUT_DIR/${basename}_processed.json"
  
  process_file "$file" "$output_file" || continue
done

# Generate summary
$MYAPP_BIN summarize "$OUTPUT_DIR"/*.json > "$OUTPUT_DIR/daily-summary.txt"
```

### Python Integration

```python
#!/usr/bin/env python3
# fry-wrapper.py - Python wrapper for fry

import subprocess
import json
import sys
from pathlib import Path

class MyAppWrapper:
    def __init__(self, binary_path="fry"):
        self.binary = binary_path
    
    def run_command(self, *args, input_data=None):
        """Run fry with given arguments."""
        cmd = [self.binary] + list(args)
        
        result = subprocess.run(
            cmd,
            input=input_data,
            capture_output=True,
            text=True
        )
        
        if result.returncode != 0:
            raise RuntimeError(f"Command failed: {result.stderr}")
        
        return result.stdout
    
    def process_json(self, data):
        """Process data and return JSON output."""
        json_str = json.dumps(data)
        output = self.run_command(
            "process",
            "--format", "json",
            input_data=json_str
        )
        return json.loads(output)
    
    def batch_process(self, files):
        """Process multiple files in batch."""
        results = []
        
        for file_path in files:
            try:
                output = self.run_command("process", str(file_path))
                results.append({
                    "file": str(file_path),
                    "status": "success",
                    "output": output
                })
            except RuntimeError as e:
                results.append({
                    "file": str(file_path),
                    "status": "error",
                    "error": str(e)
                })
        
        return results

# Example usage
if __name__ == "__main__":
    app = MyAppWrapper()
    
    # Process all .dat files in current directory
    dat_files = Path(".").glob("*.dat")
    results = app.batch_process(dat_files)
    
    # Print summary
    successful = sum(1 for r in results if r["status"] == "success")
    print(f"Processed {successful}/{len(results)} files successfully")
```

### Makefile Integration

```makefile
# Makefile - Build automation with fry

MYAPP := fry
DATA_DIR := data
OUTPUT_DIR := output
SOURCES := $(wildcard $(DATA_DIR)/*.txt)
OUTPUTS := $(patsubst $(DATA_DIR)/%.txt,$(OUTPUT_DIR)/%.json,$(SOURCES))

.PHONY: all clean validate process

all: process

# Create output directory
$(OUTPUT_DIR):
	mkdir -p $@

# Validate all input files
validate: $(SOURCES)
	@echo "Validating input files..."
	@for file in $^; do \
		$(MYAPP) validate "$$file" || exit 1; \
	done
	@echo "All files valid."

# Process files
$(OUTPUT_DIR)/%.json: $(DATA_DIR)/%.txt | $(OUTPUT_DIR)
	$(MYAPP) process $< --output $@ --format json

process: validate $(OUTPUTS)
	@echo "Processing complete. Generating summary..."
	@$(MYAPP) summarize $(OUTPUT_DIR)/*.json > $(OUTPUT_DIR)/summary.txt

clean:
	rm -rf $(OUTPUT_DIR)

# Watch for changes and reprocess
watch:
	while true; do \
		make process; \
		inotifywait -e modify $(DATA_DIR)/*.txt; \
	done
```

## Integration Patterns

### Docker Integration

```dockerfile
# Dockerfile
FROM alpine:latest

RUN apk add --no-cache ncurses

COPY fry /usr/local/bin/
COPY process.sh /usr/local/bin/

ENTRYPOINT ["fry"]
CMD ["--help"]
```

```bash
# Build and run in Docker
docker build -t fry .

# Process files in container
docker run -v $(pwd)/data:/data fry process /data/input.txt

# Interactive mode
docker run -it fry interactive
```

### Systemd Service

```ini
# /etc/systemd/system/fry-monitor.service
[Unit]
Description=MyApp Monitoring Service
After=network.target

[Service]
Type=simple
User=fry
ExecStart=/usr/local/bin/fry monitor --daemon
Restart=on-failure
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

### Cron Job

```bash
# Process files every hour
0 * * * * /usr/local/bin/fry process /var/data/*.new --output /var/processed/

# Daily summary at 2 AM
0 2 * * * /usr/local/bin/fry summarize --yesterday | mail -s "Daily Summary" admin@example.com

# Cleanup old files weekly
0 3 * * 0 find /var/processed -mtime +30 -name "*.json" | xargs /usr/local/bin/fry archive
```

## Performance Tips

### Parallel Processing

```bash
# GNU Parallel for maximum throughput
find . -name "*.dat" | parallel -j 8 fry process {} --output {.}.json

# Process with progress bar
find . -name "*.dat" | parallel --bar fry process {} :::: -

# Limit memory usage
find . -name "*.dat" | parallel --memfree 1G fry process {}
```

### Batch Operations

```bash
# Process in chunks to reduce overhead
find . -name "*.dat" -print0 | xargs -0 -n 100 fry process --batch

# Use named pipes for streaming
mkfifo /tmp/fry-pipe
fry monitor --output /tmp/fry-pipe &
cat /tmp/fry-pipe | fry analyze --stream
```

### Resource Management

```bash
# Limit CPU usage
nice -n 10 fry process large-dataset.dat

# Limit memory
ulimit -v 1048576 && fry process --low-memory

# Monitor resource usage
/usr/bin/time -v fry process large-file.dat
```

## Error Handling

### Robust Scripts

```bash
#!/bin/bash
# robust-processor.sh - Error-handling example

set -euo pipefail
trap 'echo "Error on line $LINENO"' ERR

# Function with error handling
safe_process() {
  local file="$1"
  local max_retries=3
  local retry_count=0
  
  while [ $retry_count -lt $max_retries ]; do
    if fry process "$file" 2>/tmp/fry-error.log; then
      return 0
    else
      retry_count=$((retry_count + 1))
      echo "Attempt $retry_count failed for $file"
      sleep $((retry_count * 2))
    fi
  done
  
  echo "Failed to process $file after $max_retries attempts"
  cat /tmp/fry-error.log
  return 1
}

# Process with error recovery
for file in *.dat; do
  if ! safe_process "$file"; then
    echo "$file" >> failed-files.txt
  fi
done

# Report results
if [ -f failed-files.txt ]; then
  echo "Failed files:"
  cat failed-files.txt
  exit 1
fi
```

### Validation Pipeline

```bash
# Multi-stage validation
validate_and_process() {
  local input="$1"
  
  # Stage 1: Format validation
  if ! fry validate --format "$input"; then
    echo "Format validation failed" >&2
    return 1
  fi
  
  # Stage 2: Content validation
  if ! fry validate --content "$input"; then
    echo "Content validation failed" >&2
    return 2
  fi
  
  # Stage 3: Process with verification
  local output=$(mktemp)
  if fry process "$input" --output "$output"; then
    if fry verify "$output"; then
      mv "$output" "${input%.dat}.json"
      return 0
    else
      echo "Output verification failed" >&2
      rm -f "$output"
      return 3
    fi
  else
    echo "Processing failed" >&2
    rm -f "$output"
    return 4
  fi
}
```

## Best Practices

1. **Always check exit codes** when scripting
2. **Use proper quoting** for filenames with spaces
3. **Handle signals gracefully** in long-running operations
4. **Log operations** for debugging and auditing
5. **Validate input** before processing
6. **Use atomic operations** for file updates
7. **Implement retry logic** for transient failures
8. **Monitor resource usage** for large datasets

## See Also

- [Basic Usage](adding-a-command.md)
- [Configuration Guide](config.json)
- [API Documentation](../docs/README.md)