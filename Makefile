all:
	@echo ""
	@echo "usages: make { clean | build | envsetup | release }"
	@echo "ex: make release"
	@echo ""

clean:
	@echo ""
	@echo "Cleaning the project"
	@rm -rf libnetconf2/build/ libssh/build/ netopeer2/build/ libruapp/build/ libyang/build/ sysrepo/build/
	@echo "Done"
	@echo ""

build:
	@echo ""
	@echo "building and installing dependencies"
	@./netopeer2_install.sh --build
	@echo done
	@echo ""

envsetup:
	@echo ""
	@echo "Setting up the development env"
	@./netopeer2_install.sh --setup
	@echo done
	@echo ""

release:
	@echo ""
	@echo "Packing the release binary"
	@./bin_pack.sh
	@echo "Done"
	@echo ""
