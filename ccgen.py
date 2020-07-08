#!/usr/bin/env python3

import sys
import CppHeaderParser
import time
import jinja2
import os
import json

global f_log

def log(string):
    'ccgen: {}'.format(string)
    f_log.write(f'ccgen:\t{string}\n')

def fail(string):
    log(string)
    sys.exit(1)


def uncomment(comment):
    lines = comment.splitlines()
    for line in lines:
        line = line.lstrip()

        # TODO: correct doxygen style checks instead of reduced
        if line.startswith('///') or line.startswith('//!'):
            line = line[3:].strip()
    return '\n'.join(lines)


def collect_enum(enum, scope):
    enums = {}

    enum_docstring = enum.get('doxygen', '')
    enum_name = enum.get('name', '')
    enum_namespace = enum.get('namespace', '')

    if scope:
        enum_fqname = f'{scope}::{enum_name}'
    elif enum_namespace:
        enum_fqname = f'{enum_namespace}{enum_name}'
    else:
        enum_fqname = enum_name

    log(f'found enum {enum_fqname} with docstring \'{enum_docstring}\'')

    if enum_docstring and enum_name:
        enum_values = []
        for value in enum.get('values'):
            vname = value.get('name')
            if vname:
                log(f'found enum value {enum_fqname}::{vname}')
                enum_values.append(vname)

        enums[enum_fqname] = {'name': enum_fqname, 'values': enum_values}

    return enums


def collect_class(class_, scope):

    classes = {}
    enums = {}

    class_docstring = class_.get('doxygen', '')
    class_name = class_.get('name', '')
    class_namespace = class_.get('namespace', '')

    class_scope = scope if scope else class_namespace
    class_fqname = f'{class_scope}::{class_name}' if class_scope else class_name

    log(f'found class: {class_fqname} with docstring \'{class_docstring}\'')

    if not class_name:
        log(f'warning: cannot generate code for anonymous class')
    else:

        class_parents = [parent.get('class') for parent in class_.get('inherits', [])]

        # process siblings before checking docstring, because the parent class can be not tagged for code generation
        for nested_class in class_.get('nested_classes', []):
            (nested_classes, nested_enums) = collect_class(nested_class, class_fqname)
            classes = {**classes, **nested_classes}
            enums = {**enums, **nested_enums}

        # nested enums
        for (access, class_enums) in class_.get('enums', {}).items():
            for enum in class_enums:
                enums = {**enums, **collect_enum(enum, class_fqname)}

        if class_docstring:
            # attributes
            class_attributes = []
            for (access, attributes) in class_.get('properties', {}).items():
                for attribute in attributes:
                    attribute_docstring = attribute.get('doxygen', '')
                    if not attribute_docstring:
                        continue

                    attribute_name = attribute.get('name', '')
                    attribute_type = attribute.get('type', '')

                    class_attributes.append({
                        'name': attribute_name,
                        'type': attribute_type,
                        'doc': attribute_docstring,
                        'access': access,
                    })

            classes[class_fqname] = {
                'name': class_fqname,
                'attributes': class_attributes,
                'parents': class_parents,
                'doc': class_docstring,
                'scope': class_scope,
            }

    return classes, enums


def resolve_parent_name(class_names, name, scope):

    if scope:
        if scope.startswith('::'):
            scope = scope[2:]

        scoped_name = f'{scope}::{name}'
        if scoped_name in class_names:
            return scoped_name

    if name in class_names:
        return name

    if name.startswith('::') and name[2:] in class_names:
        return name

    log(f'cannot resolve base class name {name} in scope {scope}')
    return ''


def correct_parent_names(classes):
    # remove not exported parents in two passes
    exported_class_names = classes.keys()

    # correct parent names to fully-qualified
    for class_ in classes.values():
        scope = class_['scope']
        class_['parents'] = [s for s in (resolve_parent_name(exported_class_names, p, scope) for p in class_['parents']) if s]

    # remove untagged parent class names
    for class_ in classes.values():
        class_['parents'] = [s for s in class_['parents'] if s in exported_class_names]


def get_source_information(infile):

    classes = {}
    enums = {}

    start_time = time.perf_counter()
    parser = CppHeaderParser.CppHeader(infile, 'file')
    finish_time = time.perf_counter()
    log(f'parse {infile}: done in {finish_time - start_time}s')

    # enums
    for enum in parser.enums:
        enums = {**enums, **collect_enum(enum, '')}

    # classes
    for class_ in parser.classes.values():
        (tmp_classes, tmp_enums) = collect_class(class_, '')
        enums = {**enums, **tmp_enums}
        classes = {**classes, **tmp_classes}

    return {'classes': classes, 'enums': enums}


def collect_source_information(infiles):

    classes = {}
    enums = {}
    files = []

    for infile in (os.path.normpath(f) for f in infiles):
        info = get_source_information(infile)
        tmp_classes = info.get('classes')
        tmp_enums = info.get('enums')

        if tmp_classes or tmp_enums:
            files.append(infile)

        classes = {**classes, **tmp_classes}
        enums = {**enums, **tmp_enums}

    correct_parent_names(classes)

    return {'files': files, 'classes': classes, 'enums': enums}


if __name__ == '__main__':
    f_log = open('ccgen.log', 'w')
# argv[0] -> filename
# argv[1] -> output directory
# argv[2:] -> input file names

    if len(sys.argv) < 3:
        fail('no input file specified')

    infiles = sys.argv[2:]
    source_information = collect_source_information(infiles)

    with open('ccgen.json', 'w') as f_json:
        json.dump(source_information, f_json, indent=4, sort_keys=True)

    templates_dir = os.path.join(os.path.dirname(sys.argv[0]), 'templates')
    out_dir = sys.argv[1]

    jinja_env = jinja2.Environment(loader=jinja2.FileSystemLoader(templates_dir))

    signed_integral_types = ['int8_t', 'int16_t', 'int32_t', 'int64_t']
    unsigned_integral_types = ['uint8_t', 'uint16_t', 'uint32_t', 'uint64_t']
    floating_point_types = ['float', 'double']
    warning = \
        "// Warning:\n"\
        "// This file is autogenerated at build phase\n"\
        "// All changes made will be lost on next rebuild\n"

    template_args = {
        'files': source_information['files'],
        'classes': source_information['classes'],
        'enums': source_information['enums'],
        #'prefix': os.path.basename(out_prefix) + '.',
        #'signed_integral_types': signed_integral_types,
        #'unsigned_integral_types': unsigned_integral_types,
        #'floating_point_types': floating_point_types,
        #'arithmetic_types': signed_integral_types + unsigned_integral_types + floating_point_types,
        'warning': warning,
	}

    with open('ccgen_dump.txt', 'w') as f_dump:
        f_dump.write('affected headers:\n\n')
        for file in template_args['files']:
            f_dump.write(f'header: {file}\n')
        f_dump.write('\n')

        f_dump.write('affected enums:\n\n')
        for enum in template_args.get('enums', {}).values():
            f_dump.write(f'enum: {enum["name"]}\n')
            for value in enum.get('values', []):
                f_dump.write(f'  value: {value}\n')
            f_dump.write('\n')

        f_dump.write('affected classes:\n\n')
        for class_ in template_args.get('classes', {}).values():
            f_dump.write(f'class: {class_["name"]}\n')
            for attribute in class_['attributes']:
                f_dump.write(f'  attribute: {attribute["name"]}\n')
            f_dump.write('\n')

    for template_name in jinja_env.list_templates():
        template = jinja_env.get_template(template_name)
        out_path = os.path.join(out_dir, os.path.basename(template.filename))
        #out_file = os.path.basename(out_path)
        out_data = template.render(**template_args)
        if out_data:
            with open(out_path, 'w') as f_out:
                f_out.write(out_data)

    f_log.close()
    sys.exit(0)
