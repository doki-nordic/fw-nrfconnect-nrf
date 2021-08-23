
'''
TODO before this script can be production-ready:
 * Reading search index independent from Node.js
 * Checking integrity of the index after/during loading it: in case format changes in newer version of sphinx
 * More checking if items are already in the index, so merging the same file twice is not an error
 * CLI
'''

import subprocess
import json

def read_search_index(file):
    json_text = subprocess.check_output(['node', '-e', 'file="' + file + '";Search={setIndex:x=>process.stdout.write(JSON.stringify(x))};eval(require("fs").readFileSync(file,"utf-8"));'])
    return json.loads(json_text)

def write_search_index(index, file):
    f = open(file, "w")
    f.write('Search.setIndex(')
    f.write(json.dumps(index, sort_keys=True, indent=4))
    f.write(');')
    f.close()

def prefix_titles(dst, title_prefix):
    dst['titles'] = [title_prefix + x for x in dst['titles']]

def merge_docnames(dst, src, relative_path, title_prefix):
    result = len(dst['docnames'])
    for name in src['docnames']:
        dst['docnames'].append(relative_path + '/' + name)
    for name in src['filenames']:
        dst['filenames'].append(relative_path + '/' + name)
    for name in src['titles']:
        dst['titles'].append(title_prefix + name)
    return result

def merge_terms(dst, src, key, docnames_offset):
    for term, value in src[key].items():
        if term in dst[key]:
            if type(value) is list:
                refs = [ x + docnames_offset for x in value ]
            else:
                refs = [ value + docnames_offset ]

            if (type(dst[key][term]) is not list):
                dst[key][term] = [ dst[key][term] ]

            dst[key][term] += refs
        else:
            dst[key][term] = value

def merge_objnames(dst, src):
    objnames_map = {}
    for src_name, src_value in src['objnames'].items():
        found = False
        max_name = -1
        for dst_name, dst_value in dst['objnames'].items():
            max_name = max(max_name, int(dst_name))
            if src_value == dst_value and src['objtypes'][src_name] == dst['objtypes'][dst_name]:
                objnames_map[src_name] = dst_name
                found = True
        if not found:
            dst_name = str(max_name + 1)
            objnames_map[src_name] = dst_name
            dst['objnames'][dst_name] = src_value
            dst['objtypes'][dst_name] = src['objtypes'][src_name]
    return objnames_map

def merge_object(dst_object, src_object, docnames_offset, objnames_map):
    for src_name, src_value in src_object.items():
        dst_object[src_name] = [
            src_value[0] + docnames_offset,
            objnames_map[str(src_value[1])],
            src_value[2],
            src_value[3]
        ]

def merge_objects(dst, src, docnames_offset, objnames_map):
    for src_prefix, src_object in src['objects'].items():
        if src_prefix not in dst['objects']:
            dst['objects'][src_prefix] = {}
        merge_object(dst['objects'][src_prefix], src_object, docnames_offset, objnames_map)

def merge_search_index(dst, file, relative_path, title_prefix):
    src = read_search_index(file)
    docnames_offset = merge_docnames(dst, src, relative_path, title_prefix)
    merge_terms(dst, src, 'terms', docnames_offset)
    merge_terms(dst, src, 'titleterms', docnames_offset)
    objnames_map = merge_objnames(dst, src)
    merge_objects(dst, src, docnames_offset, objnames_map)

index = read_search_index('/home/doki/work/fmn/nrf/doc/_build/html/mcuboot/searchindex-org.js')
write_search_index(index, '/home/doki/work/fmn/nrf/doc/_build/html/mcuboot/searchindex-1.js')
prefix_titles(index, 'MCUboot » ')
merge_search_index(index, '/home/doki/work/fmn/nrf/doc/_build/html/nrfxlib/searchindex.js', '../nrfxlib', 'nrfxlib » ')

#index = read_search_index('/home/doki/work/fmn/nrf/doc/_build/html/nrfxlib/searchindex.js')
#merge_search_index(index, '/home/doki/work/fmn/nrf/doc/_build/html/mcuboot/searchindex.js', '../mcuboot')

write_search_index(index, '/home/doki/work/fmn/nrf/doc/_build/html/mcuboot/searchindex.js')
