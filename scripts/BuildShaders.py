import hashlib
import sys
import io
import os
import re
import argparse
import binascii

compiled_shader_extensions = ['.vertex', '.pixel', '.compute']

do_log_verbose = False

# todo: relative/system.
# https://www.wihlidal.com/blog/pipeline/2018-10-04-parsing-shader-includes/
include_regex = re.compile(r'(?m)^\#include\s+["<]([^">]+)*[">]', )

def log(msg):
    print(msg)

def log_verbose(msg):
    if do_log_verbose:
        log(msg)


def calc_shader_digest(shader_path, shader_src_dir, include_hashes):
    hasher = hashlib.sha256()

    with open(shader_path, 'r') as f:
        contents = f.read()

        # hash the text.
        hasher.update(contents.encode('utf-8'))

        includes = include_regex.findall(contents)
        for include in includes:
            include_path = os.path.abspath(os.path.join(os.path.dirname(shader_path), include))
            if not os.path.exists(include_path):
                log('Can\'t find include {} in file {}'.format(include, shader_path))
                continue         

            if include_path in include_hashes:
                hasher.update(include_hashes.get(include_path))
                continue

            # not in cache, hash the include.
            log_verbose("Hashing include: {}".format(include_path))
            digest = calc_shader_digest(include_path, shader_src_dir, include_hashes)
            hasher.update(digest)
            include_hashes[include_path] = digest
                   
    return hasher.digest()

def handle_shader(shader_path, shader_base_dir, include_hashes, shaders_to_rebuild):
    hash_file_path = shader_path + '.hash'

    old_hash = ''

    try:
        with open(hash_file_path, 'r') as hash_file:
            old_hash_hex = hash_file.read()
            old_hash = binascii.a2b_hex(old_hash_hex)
    except:
        log("No hash file found for shader {}".format(shader_path))
        shaders_to_rebuild.append(shader_path)

    new_hash = calc_shader_digest(shader_path, shader_base_dir, include_hashes)
    
    if old_hash != new_hash:
        log("Shader hash for {} changed.".format(shader_path))
        shaders_to_rebuild.append(shader_path)
        with open(hash_file_path, 'w') as hash_file:
            hash_file.write(binascii.b2a_hex(new_hash).decode('ascii'))
    else:
        log_verbose('Shader {} didn\'t change, won\'t rebuild.'.format(shader_path))

def parse_shader_tree(shader_src_dir, shaders_to_rebuild):
    include_hashes = dict()

    for dirName, subDirs, files in os.walk(shader_src_dir):
        for filename in files:
            path_to_shader = os.path.join(dirName, filename)
            if os.path.splitext(filename)[1] in compiled_shader_extensions:
                log_verbose('Checking shader {}'.format(path_to_shader))
                handle_shader(path_to_shader, shader_src_dir, include_hashes, shaders_to_rebuild)
        

def main():
    arsparser = argparse.ArgumentParser(description='Build shaders.')
    arsparser.add_argument('--verbose', action='store_true')
    args = arsparser.parse_args()

    global do_log_verbose
    do_log_verbose = args.verbose

    script_dir = os.path.dirname(__file__) 
    pathos_base = os.path.normpath(os.path.join(script_dir, '../'))
    shader_src_dir = os.path.join(pathos_base, 'assets/shaders/')
    shader_out_dir = os.path.join(pathos_base, 'run_tree/shaders/')

    shaders_to_rebuild = []
    parse_shader_tree(shader_src_dir, shaders_to_rebuild)

    for shader in shaders_to_rebuild:
        log("Rebuilding shader: {}".format(shader))



if __name__ == "__main__":
    main()