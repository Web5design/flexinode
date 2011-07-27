<?php
// $Id: Exp $

/**
 * @file mfx.field.inc
 *
 * Provides the embeded field to be used in fieldable entities.
 *
 * @author : René Bakx (rene@71media.net)
 *
 */


/**
 * Implements hook_field_info().
 *
 * Hook in the new flexifield content type
 * @return array
 */

function mfx_field_info() {
    $fields = array();
    $fields['flexifield'] = array(
        'label' => t('Flexifield'),
        'description' => t('This field internaly embeds the edit form from a flexinode'),
        'settings' => array('referenceable_types' => array()),
        'default_widget' => 'mfx_flexifield_widget',
        'default_formatter' => 'mfx_flexifield_formatter',
        'property_type' => 'flexinode',
    );
    return $fields;
}

/**
 * Implements hook_schema().
 * Defines storage for flexifield in host entity
 * @param $field
 * @return array
 */
function mfx_field_schema($field) {
    if ($field['type'] == 'flexifield') {
        $columns = array(
            'flid' => array(
                'type' => 'int',
                'unsigned' => TRUE,
                'not null' => TRUE,
            ),
            'nid' => array(
                'type' => 'int',
                'unsigned' => TRUE,
                'not null' => TRUE,
            ),
            'type' => array(
                'type' => 'varchar',
                'length' => 255,
                'not null' => TRUE,
            ),
        );
        return array(
            'columns' => $columns,
            'indexes' => array('nid' => array('nid')),
            'foreign keys' => array(
                'nid' => array(
                    'table' => 'node',
                    'columns' => array('nid' => 'nid'),
                ),
            ),
        );
    }
}

function mfx_field_settings_form($field, $instance, $has_data) {
    if ($field['type'] == 'flexifield') {
        $available_types = mfx_flexinode_get_types();
        $options = array();
        foreach ($available_types as $type) {
            $options += array($type->type => check_plain($type->label));
        }
        unset($available_types[$instance['bundle']]); // no self references
        $settings = $field['settings'];
        $form = array();
        $form['referenceable_types'] = array(
            '#type' => 'checkboxes',
            '#title' => t('Content types that can be referenced'),
            '#multiple' => TRUE,
            '#default_value' => is_array($settings['referenceable_types']) ? $settings['referenceable_types']
                    : array(),
            '#options' => $options,
            '#disabled' => $has_data,
        );
        return $form;
    }
}

function mfx_field_is_empty($item, $field) {
    return false;
}

function mfx_field_widget_info() {
    return array(
        'mfx_flexifield_widget' => array(
            'label' => t('Flexifield widget'),
            'description' => t('Show the entry form for a widget'),
            'field types' => array('flexifield'),
            'settings' => array(),
            'behaviors' => array(
                'multiple values' => FIELD_BEHAVIOR_DEFAULT,
                'default value' => FIELD_BEHAVIOR_NONE, // no default values for this
            ),
        )
    );
}


function mfx_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {
    $return = false;
    if ($instance['widget']['type'] == 'mfx_flexifield_widget') {
        if (!in_array('mfx_flexifield_widget_submit', $form['#submit'])) {
            $form['#submit'][] = 'mfx_flexifield_widget_submit';
        }
        $elementName = 'flexi_container_' . $field['field_name'];
        $targetElement = 'ajax_' . $elementName . '_' . $delta;
        $field_name = $element['#field_name'];

        $element[$elementName] = array(
            '#field_name' => $elementName,
            '#type' => 'container',
            '#title' => t('flexifield item'),
            '#weight' => $delta,
            '#delta' => $delta,
            '#tree' => true,
            '#language' => $langcode,
            '#element_validate' => array('mfx_flexifield_widget_validate'),
            '#prefix' => '<div id="' . $targetElement . '">',
            '#suffix' => '</div>'
        );

        $available_flexinode_types = array_map('check_plain', array_merge(array(null => t('none')), $field['settings']['referenceable_types']));
        foreach ($available_flexinode_types as $key => $reference) {
            if ($reference == '0') {
                unset($available_flexinode_types[$key]);
            }
        }

        $element[$elementName]['select'] = array(
            '#delta' => $delta,
            '#type' => 'select',
            '#required' => false,
            '#default_value' => isset($items[$delta]['type']) ? $items[$delta]['type'] : NULL,
            '#options' => $available_flexinode_types,
            '#ajax' => array(
                'callback' => 'mfx_flexifield_widget_trigger_subform_ajax',
                'wrapper' => $targetElement,
                'method' => 'replace',
                'effect' => 'slide',
                'progress' => array('type' => 'throbber'),
            )
        );

        // embed on AJAX callback..
        $parents = array_merge($element['#field_parents'], array($field_name, $langcode, $delta, $elementName, 'content'));

        $ajaxtriggered = (isset($form_state['triggering_element'])) ? true : false;
        $flexinodetype = (isset($form_state['triggering_element']["#value"])) ? $form_state['triggering_element']["#value"] : NULL;
        $triggeringdelta = (isset($form_state['triggering_element']['#delta'])) ? $form_state['triggering_element']['#delta'] : NULL;
        $field_state = field_form_get_state($parents, $field_name, $langcode, $form_state);

        // this is a new or to be changed form
        if ($triggeringdelta == $delta) {
            $flexinode = mfx_field_get_entity($items[$delta], $flexinodetype);
            if (is_object($flexinode)) {
                flexinode_get_field_form($form_state, $element, $elementName, $flexinode, $delta, $field_name, $langcode, $parents);
            } elseif ($ajaxtriggered && !empty($flexinodetype)) {
                // object was cleared, so remove from state
                $field_state['entity'][$delta] = $flexinode;
                $field_state['subform'][$delta] = $element[$elementName]['content'];
                field_form_set_state($parents, $field_name, $langcode, $form_state, $field_state);
            }
        } elseif (isset($field_state[$delta])) {
            // store cached object to form
            $element += $field_state[$delta];
        }
        $return = $element;
    }
    return $return;
}

function flexinode_get_field_form(&$form_state, &$element, $elementName, $flexinode, $delta, $field_name, $langcode, $parents) {
    $element[$elementName]['content'] = array(
        '#field_name' => 'content',
        '#delta' => $delta,
        '#type' => 'container',
        '#tree' => true,
        '#language' => $langcode,
        '#field_parents' => $parents,
        '#parents' => $parents,

    );

    field_attach_form('flexinode', $flexinode, $element[$elementName]['content'], $form_state, $langcode);
    // store a copy of the form and entity in the form_state
    $field_state = field_form_get_state($parents, $field_name, $langcode, $form_state);
    $field_state['entity'][$delta] = $flexinode;
    $field_state['subform'][$delta] = $element[$elementName]['content'];
    field_form_set_state($parents, $field_name, $langcode, $form_state, $field_state);
}

function mfx_form_alter(&$form, &$form_state) {
    if ($form['#node_edit_form']) {
        #sdpm($form);
    }
}

function mfx_flexifield_widget_trigger_subform_ajax($form, &$form_state) {
    $element = $form_state['triggering_element'];
    array_pop($element['#parents']);
    $subform = drupal_array_get_nested_value($form, $element['#parents']);
    return $subform;
}

function mfx_flexifield_widget_validate($element, &$form_state, &$form) {
    if ($form_state['submitted']) {
        $element_name = reset($element['#parents']);
        $delta = $element['#delta'];
        $parents = array_merge($element['#parents'], array('content', '#fields', $element_name, $element['#language']));
        $field_state = drupal_array_get_nested_value($form_state['field']['#parents'], $parents);
        
        dpr($field_state);

        field_attach_submit('flexinode', $flexinode, $element, $form_state);
        $item['entity'] = $flexinode;
        form_set_value($element, $item, $form_state);
        #dpr($field_state['#fields'][$element_name][$element['#language']]);
    }


    //
    //
    //    $flexinode = $field_state['entity'];
    //    // Attach field API validation of the embedded form.
    //    field_attach_form_validate('flexinode', $flexinode, $element, $form_state);

    // TODO subform validation!
    //    if (!field_collection_item_is_empty($flexinode_entity) && !empty($element['#field_collection_required_elements'])) {
    //        foreach ($element['#field_collection_required_elements'] as &$elements) {
    //            // Copied from _form_validate().
    //            if (isset($elements['#needs_validation'])) {
    //                $is_empty_multiple = (!count($elements['#value']));
    //                $is_empty_string = (is_string($elements['#value']) && drupal_strlen(trim($elements['#value'])) == 0);
    //                $is_empty_value = ($elements['#value'] === 0);
    //                if ($is_empty_multiple || $is_empty_string || $is_empty_value) {
    //                    if (isset($elements['#title'])) {
    //                        form_error($elements, t('!name field is required.', array('!name' => $elements['#title'])));
    //                    }
    //                    else {
    //                        form_error($elements);
    //                    }
    //                }
    //            }
    //        }
    //    }

    //    if ($form_state['submitted'] && !form_get_errors() && is_object($flexinode)) {
    //        field_attach_submit('flexinode', $flexinode, $element, $form_state);
    //        $item['entity'] = $flexinode;
    //        form_set_value($element, $item, $form_state);
    //    }
}


function mfx_flexifield_widget_submit(&$form, &$form_state) {
    echo "values";
    dpr($form_state['values']['field_flexi']['und']);
    exit();
}


function mfx_field_get_entity(&$item, $type = NULL) {
    if (isset($item['flid'])) {
        // todo load ;)
    }
    elseif (!isset($item['entity']) && isset($type)) {
        $item['entity'] = entity_create('flexinode', array('type' => $type));
    }
    return isset($item['entity']) ? $item['entity'] : FALSE;
}