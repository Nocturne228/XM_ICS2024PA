STUID = 8204212223
STUNAME = Xiaoma

# DO NOT modify the following code!!!

GITFLAGS = -q --author='tracer-ics2024 <tracer@njuics.org>' --no-verify --allow-empty

# prototype: git_commit(msg)
define git_commit
	# 
endef

_default:
	@echo "Please run 'make' under subprojects."

# submit:
# 	git gc
# 	STUID=$(STUID) STUNAME=$(STUNAME) bash -c "$$(curl -s http://why.ink:8080/static/submit.sh)"

.PHONY: default submit
