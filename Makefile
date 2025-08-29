#CXX = g++ 
#-g
CXX = g++ -gdwarf-2 -gstrict-dwarf -fvar-tracking-assignments  
TARGET = GNN

SRC_DIR = ./src \
          ./src/Buffer \
		  ./src/Caculate \
		  ./src/common \
		  ./src/dram \
		  ./src/event \
		  ./src/Observer


INC_DIR = ./src \
          ./src/Buffer \
		  ./src/Caculate \
		  ./src/common \
		  ./src/dram \
		  ./src/event \
		  ./src/Observer
        #  ./inc/dma_inc \
        
INC_DIR_DRAM = ./DRAMsim3-master/src \
               ./DRAMsim3-master/ext/fmt/include \
			   ./DRAMsim3-master/ext/headers
OBJ_DIR = .obj
LIB_DIR = ./DRAMsim3-master

SRC = $(shell find $(./src) -type f -name '*.cpp')
INC = $(shell find $(./src) -type f -name '*.h') $(wildcard $(INC_DIR_DRAM)/*.h)
#INC = $(wildcard $(INC_DIR)/*.h) $(wildcard $(INC_DIR_DRAM)/*.h)
OBJ = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRC))
#all:
#	@echo $(INC)
CXXFLAGS = -c -Wall $(addprefix -I,$(INC_DIR_DRAM)) $(addprefix -I,$(INC_DIR))-std=c++11
LDFLAGS  = -L$(LIB_DIR) -ldramsim3 -Wl,-rpath $(LIB_DIR)
#all:
#	@echo $(CXXFLAGS)
$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) $(addprefix -I,$(INC_DIR_DRAM)) $(addprefix -I,$(INC_DIR)) -o $@ $^ 

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(INC)
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@
.PHONY: clean
clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET)

