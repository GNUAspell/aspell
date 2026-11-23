ASPELL ?= aspell
PREFIX ?= /usr
DESTDIR ?=
dictdir ?= $(shell $(ASPELL) config dict-dir)
datadir ?= $(shell $(ASPELL) config data-dir)

.PHONY: all check install clean
all: sv.rws
sv.rws: sv.wl sv.dat sv_phonet.dat
	$(ASPELL) --lang=sv --encoding=utf-8 --local-data-dir=$(CURDIR) --dict-dir=$(CURDIR) create master $@ < sv.wl
check: all
	python3 -m unittest discover -s tests -p 'test_*.py' -v
	python3 tests/check_dictionary.py --local .
install: all
	install -d $(DESTDIR)$(dictdir) $(DESTDIR)$(datadir)
	install -m 644 sv.rws sv.multi $(DESTDIR)$(dictdir)/
	install -m 644 sv.dat sv_phonet.dat $(DESTDIR)$(datadir)/
clean:
	rm -f sv.rws sv.cwl.gz
	rm -rf tests/__pycache__ tools/__pycache__
