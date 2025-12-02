# Makefile simple raíz

MODULES = utils master worker storage query_control

.PHONY: all clean $(MODULES) $(addprefix clean-,$(MODULES))

all: $(MODULES)
	@echo "Compilación completa."

clean: $(addprefix clean-,$(MODULES))
	@echo "Limpieza completa."

$(MODULES):
	@echo "Compilando $@..."
	@cd $@ && $(MAKE)

$(addprefix clean-,$(MODULES)):
	@echo "Limpiando $(subst clean-,,$@)..."
	@cd $(subst clean-,,$@) && $(MAKE) clean || true