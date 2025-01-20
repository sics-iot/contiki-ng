# Default target
all: install generate

# Create a virtual environment
.PHONY: venv
venv:
	python3 -m venv venv

# Install dependencies
.PHONY: install
install: venv
	@echo "Installing dependencies"
	. venv/bin/activate && venv/bin/pip install -r requirements.txt

# Generate headers
.PHONY: generate
generate: venv
	@echo "Generating Headers"
	. venv/bin/activate && cd c && ./c_header_coap.py

# Update dependencies
.PHONY: update
update: venv
	@echo "Updating dependencies"
	. venv/bin/activate && venv/bin/pip install --upgrade -r requirements.txt

# Clean
.PHONY: clean
clean:
	@echo "Cleaning generated files"
	rm -rf ./c/cache/*

# Help target
.PHONY: help
help:
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  install     : Create virtual environment and install dependencies"
	@echo "  update      : Update project dependencies"
	@echo "  generate    : Generate headers"
	@echo "  clean       : Clean generated files"
	@echo "  help        : Display this help message"
