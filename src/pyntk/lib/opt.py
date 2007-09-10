class OptErr(Exception):pass

class Opt:
    def __getattr__(self, str):
        return None

OptFileCode = 'class OptFile(Opt): execfile(path)'
def opt_load_file(path):
    exec(OptFileCode)
    return OptFile
