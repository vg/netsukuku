class OptErr(Exception):pass

class Opt:
    def __init__(self):
        self.dict = {}

    def __getattr__(self, str):
        return None

    def opt_load_file(self, path):
	execfile(path, self.__dict__, self.__dict__)
