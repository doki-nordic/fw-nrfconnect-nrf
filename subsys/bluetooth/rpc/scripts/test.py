
import os
import sys

sys.path.insert(0, os.getenv('ZEPHYR_BASE') + '/scripts/kconfig')

from kconfiglib import Symbol, Choice, MENU, COMMENT, MenuNode, \
                       BOOL, TRISTATE, STRING, INT, HEX, \
                       AND, OR, \
                       expr_str, expr_value, split_expr, \
                       standard_sc_expr_str, \
                       TRI_TO_STR, TYPE_TO_STR, \
                       standard_kconfig, standard_config_filename

'''
TODO: 
    1. get list of nodes directly depending on BT_HCI, e.g. "... && BT_HCI && ..."
    2. get their values
    3. Remove BT_HCI from deps: 
        replace recursivly AND(BT_HCI, xyz) or AND(xyz, BT_HCI) -> xyz
        if top level is BT_HCI -> replace with "y"
    4. create Kconfig:
         config SOME_CONFIG
             default y # or empty if n or some other value
             depends on ... && ... # BT_HCI is removed or empty if was depending only on BT_HCI

Problems:
    - Options not depending on BT_HCI will not be synchronized
    solution: create list of options that need to be synchronized
    if option from list depends on BT_HCI, then ok, it will be synchronized by the above procedure
    if not: TODO: then what?



Wyjście to tylko Kconfig z wpisami:
    config BT_CONFIG_NAME
        default ...value...
        depends on ...wszystkie deps nodów definiujących ten symbol połączone || z usunięciem BT_HCI...

TODO: co zrobić z symbolami, które posiadają node z promptem i są niezależne od BT_HCI oraz inny node bez prompta zależny od BT_HCI

To gwaratuje, że wpis pojawi się tylko jeżeli jakikolwiek inny wpis się pojawił
i zmienia wartość na domyślną jako piewszy, więc inne domyśle wpisy będą ignorowane

Plik musi zostać dołączony i do tego jako pierwszy. takie wpisy to gwarantują:
    w wyjściowym Kconfig
    config BT_RPC_CONFIG_INCUDED_GUARD
        default y
    config BT_RPC_CONFIG_INCUDED_FIRST_GUARD
        default 2
    w Kconfig BT_RPC:
    config BT_RPC_CONFIG_INCUDED_GUARD
        bool
    config BT_RPC_CONFIG_INCUDED_FIRST_GUARD
        int
        default 1
    w Cmake BT_RPC:
        if BT_RPC_CLIENT
            if !BT_RPC_CONFIG_INCUDED_GUARD
                message BT_RPC Kconfig configuration file not include
                message When compiling BR_RPC client module, you must include Kconfig file generated during BT_RPC server module
                message ... informacja o przykładzie jak to zrobić ...
            if BT_RPC_CONFIG_INCUDED_FIRST_GUARD != 2
                message BT_RPC Kconfig configuration file not include as a first Kconfig file
                message When compiling BR_RPC client module, you must include Kconfig file before any other generated during BT_RPC server module
                message ... informacja o przykładzie jak to zrobić ...
        

'''

class ConfigExtractor:

    def __init__(self):
        self.dep_symbols = set()
        self.no_dep_symbols = set()

    def direct_dep(self, expr, config_name):
        if expr.__class__ is tuple:
            if expr[0] == AND:
                return self.direct_dep(expr[1], config_name) or self.direct_dep(expr[2], config_name)
            else:
                return False
        elif expr.__class__ is Symbol:
            return expr.name == config_name
        else:
            return False


    def extract_symbols(self, node, config_name):
        cur = node.list
        while cur is not None:
            self.extract_symbols(cur, config_name)
            cur = cur.next

        if not isinstance(node.item, Symbol):
            return

        #TODO: Check if symbol exists in a different location with different dependencies

        sym = node.item
        if self.direct_dep(node.dep, config_name):
            self.dep_symbols.add(sym.name)
            if sym.name in self.no_dep_symbols:
                print("WARNING: " + sym.name)
        else:
            self.no_dep_symbols.add(sym.name)
            if sym.name in self.dep_symbols:
                print("WARNING" + sym.name)
        


def travel(node, ind = ''):
    if isinstance(node.item, Symbol):
        print(ind + (node.item.name or '') + ': ' + (node.prompt[0] if node.prompt else ''))
        print(ind + '   >>> ' + expr_str(node.dep))
        print(ind + '       ' + str(expr_value(node.dep)))
        if node.item.user_value is not None:
            print(ind + '  ===== ' + str(node.item.user_value))
    if isinstance(node.item, Choice):
        print(ind + (node.item.name or '') + ': ' + (node.prompt[0] if node.prompt else ''))
    elif node.item == MENU:
        print(ind + 'MENU ' + (node.prompt[0] if node.prompt else ''))
    elif node.item == COMMENT:
        print(ind + 'COMMENT ' + (node.prompt[0] if node.prompt else ''))
    cur = node.list
    while cur is not None:
        travel(cur, ind + '    ')
        cur = cur.next

def extract_config(kconf):
    kconf.load_config()
    #travel(kconf.top_node)
    ce = ConfigExtractor()
    ce.extract_symbols(kconf.top_node, 'BT_HCI')
    print(ce.dep_symbols)


def _main():
    extract_config(standard_kconfig(__doc__))

if __name__ == "__main__":
    _main()
