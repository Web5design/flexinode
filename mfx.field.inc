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


define('FL_NEW', 1);
define('FL_EDIT', 2);
define('FL_EMPTY', 3);


/**
 * Implements hook_field_info().
 *
 * Hook in the new flexifield content type
 * @return array
 */

function mfx_field_info() {
    $fields = array();
    $fields['flexinode'] = array(
        'label' => t('Flexinode'),
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
    if ($field['type'] == 'flexinode') {
        $columns = array(
            'flid' => array(
                'type' => 'int',
                'unsigned' => TRUE,
                'not null' => TRUE,
            ),
            'type' => array(
                'type' => 'int',
                'unsigned' => TRUE,
                'not null' => FALSE,
            )
        );
        return array(
            'columns' => $columns,
            'indexes' => array('flid' => array('flid')),
        );
    }
}

/**
 * This form sets the parameters on a field level in the host entity.
 */
function mfx_field_settings_form($field, $instance, $has_data) {
    if ($field['type'] == 'flexinode') {
        $options = array();
        $available_types = mfx_flexinode_get_types();
        foreach ($available_types as $type) {
            $options += array($type->id => check_plain($type->label));
        }
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

/**
 * Implements hook_widget_info().
 * @return array
 */
function mfx_field_widget_info() {
    return array(
        'mfx_flexifield_widget' => array(
            'label' => t('Embedded'),
            'description' => t('Show the entry form for a widget'),
            'field types' => array('flexinode'),
            'settings' => array(),
            'behaviors' => array(
                'multiple values' => FIELD_BEHAVIOR_DEFAULT,
                'default value' => FIELD_BEHAVIOR_NONE, // no default values for this
            ),
        )
    );
}

/**
 * The actuall widget form itself, creates a $delta indexed container with a dropdown to select the
 * flexinode to embed. This container is replaced trough a AJAX call with a new subform
 * containing both the selector and the embedded form for the flexinode based on the $delta of the selector.
 *
 * @param $form array form
 * @param $form_state array form state
 * @param $field array contains all the meta information about the field itself (deleted, locked, settings, storage)
 * @param $instance array instance of current field contains label, widget type, required etc.
 * @param $langcode string current language of the item where this field is used.
 * @param $items array delta indexed assoc. array with the values from the field storage (aka DB)
 * @param $delta $delta string delta of the item this widget is loaded for.
 * @param $element array contains information about the form element for a single widgets instance. More a convience mix of the above.
 * @return array|null
 */
function mfx_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {
    $return = NULL;
    if ($instance['widget']['type'] == 'mfx_flexifield_widget') {
        if (!in_array('mfx_flexifield_widget_submit', $form['#submit'])) {
            $form['#submit'][] = 'mfx_flexifield_widget_submit';
        }

        $elementName = 'flexi_container_' . $field['field_name'];
        $targetElement = 'ajax_' . $elementName . '_' . $delta;
        $form['#flexinodefields'][$field['field_name']] = $field['field_name'];
        // element is triggered trough ajax or set defaults from loading.
        $flexinodetype = null;
        $flexinodeid = null;
        $triggeringdelta = false;

        $state = FL_EMPTY;
        //        if (isset($form_state['triggering_element']['#delta'])) {
        //            $triggeringdelta = $form_state['triggering_element']['#delta'];
        //        }
        if (isset($items[$delta]['flid'])) {
            $flexinodeid = $items[$delta]['flid'];
            $flexinodetype = $items[$delta]['type'];
            $state = FL_EDIT;
        } elseif (isset($form_state['triggering_element']["#value"])) {
            $flexinodetype = $form_state['triggering_element']["#value"];
            $state = FL_NEW;
        }


        // get available flexinodes for this field.
        $available_flexinode_types = array(null => t('none'));
        $available_flexinode_types += array_combine(array_values($field['settings']['referenceable_types']),
                                                    array_values(mfx_flexinode_get_types(null, true)));
        // Element container
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

        $element[$elementName]['select'] = array(
            '#delta' => $delta,
            '#type' => 'select',
            '#language' => $langcode,
            '#required' => false,
            '#state' => $state,
            '#flid' => $flexinodeid,
            '#default_value' => $flexinodetype,
            '#options' => $available_flexinode_types,
            '#ajax' => array(
                'callback' => 'mfx_flexifield_widget_trigger_subform_ajax',
                'wrapper' => $targetElement,
                'method' => 'replace',
                'effect' => 'slide',
                'progress' => array('type' => 'throbber'),
            )
        );
        $return = $element;
    }

    //        $field_name = $element['#field_name'];
    //        $parents = array_merge($element['#field_parents'], array($field_name, $langcode, $delta, $elementName, 'content'));
    //        // on AJAX callback add the embedded form into the widget for the current $delta
    //        $ajaxtriggered = (isset($form_state['triggering_element'])) ? true : false;

    //            $flexinode = mfx_field_get_entity($items[$delta], $flexinodetype);
    //            if (isset($items[$delta]['flid'])) {
    //                 // Edit screen..
    //                  $element[$elementName]['select']['#default_value'] = $flexinode->type;
    //            }
    //            if (is_object($flexinode)) {
    //                $element[$elementName]['content'] = array(
    //                    '#field_name' => 'content',
    //                    '#delta' => $delta,
    //                    '#type' => 'container',
    //                    '#tree' => true,
    //                    '#language' => $langcode,
    //                    '#field_parents' => $parents,
    //                    '#parents' => $parents,
    //
    //                );
    //                field_attach_form('flexinode', $flexinode, $element[$elementName]['content'], $form_state, $langcode);
    //                // store a copy of the entire widget to be restored in the form_alter hook below.
    //                $form_state['#flexinodes'][$field_name][$delta] = array('entity' => $flexinode, 'subform' => $element);
    //            } elseif ($ajaxtriggered && empty($flexinodetype)) {
    //                // object was cleared, so it must be removed from form_state too.
    //                unset($form_state['#flexinodes'][$field_name][$delta]);
    //            }
    //        }


    return $return;
}

/**
 * The actual AJAX callback, replaces the entire above created flexi_container_{field_name}
 */
function mfx_flexifield_widget_trigger_subform_ajax($form, &$form_state) {
    $element = $form_state['triggering_element'];
    array_pop($element['#parents']);
    $subform = drupal_array_get_nested_value($form, $element['#parents']);
    return $subform;
}

/**
 * Implements hook_form_alter().
 *
 * Drupal uses a static stack to keep track of the current form, however since the form is changed trough
 * AJAX multiple times, that gave some issues. PHP is stateless, and drupal static is not saved anywwere the
 * content of the static is lost between ajax calls. However the form_state is being passed around as
 * refference everywhere and even kept in the form cache, we rebuild the current form by sneeking in
 * the elements within the form_alter so that on submission all the form_state['fields'] should be present.
 *
 */
function mfx_form_alter(&$form, &$form_state) {
    if (isset($form['#node_edit_form']) && isset($form['#flexinodefields'])) {
        $language = $form['language']['#value'];
        foreach ($form['#flexinodefields'] as $field_container_name) {
            foreach ($form[$field_container_name][$language] as $key => $field_container) {
                if (is_numeric($key)) {
                    $elementName = 'flexi_container_' . $field_container['#field_name'];
                    $selectElement = $field_container[$elementName]['select'];
                    $element = $field_container[$elementName];
                    $targetParents = array($field_container['#field_name'], $element['#language'], $element['#delta'], $elementName, 'content');
                    switch ($selectElement['#state']) {

                        case FL_NEW :
                            mfx_handle_subform($form, $form_state, $element, $targetParents);
                            break;

                        case FL_EDIT:

                            break;

                        case FL_EMPTY:
                            // do nothing ;)
                            break;
                    }
                }

                // do not replace the last created AJAX element.
                //
                //                $language = $field_state['subform']['#language'];
                //                $field_state['subform']['_weight'] = $form[$element][$language][$delta]['_weight'];
                //                $form[$element][$language][$delta] = $field_state['subform'];
            }
        }
        #dpm($form['field_flexors']['und'][0]);
    }
}

function mfx_handle_subform(&$form, &$form_state, &$element, $parents) {
    $element[$element['#field_name']]['content'] = array(
        '#field_name' => 'content',
        '#delta' => $element['#delta'],
        '#type' => 'container',
        '#tree' => true,
        '#language' => $element['#language'],
        '#field_parents' => $parents,
        '#parents' => $parents,

    );
    $flexinodetype = $element['select']['#options'][$element['select']['#default_value']];
    $flexinode = mfx_field_get_entity($element['#flid'], $flexinodetype);
    field_attach_form('flexinode', $flexinode, $element[$element['#field_name']]['content'], $form_state, $element['#language']);
    drupal_array_set_nested_value($form, $parents, $element[$element['#field_name']]['content'], TRUE);

}

/**
 * implements hook_widget_validate().
 *
 * Besides validating, the embedded Flexinode is being saved to the database.
 */
function mfx_flexifield_widget_validate($element, &$form_state) {
    dpm($form_state);
    if ($form_state['submitted']) {
        $delta = $element['#delta'];
        $language = $element['#language'];
        $fieldname = reset($element['#parents']);
        /** @var $flexinode Flexinode */
        $flexinode = (isset($form_state['#flexinodes'][$fieldname][$delta]['entity'])) ? $form_state['#flexinodes'][$fieldname][$delta]['entity'] : false;
        if (is_object($flexinode)) {
            $subform = $form_state['#flexinodes'][$fieldname][$delta]['subform'][$element['#field_name']]['content'];
            $subform['#delta'] = $delta;
            #$subform_values = $form_state['values'][$fieldname][$language][$delta][$element['#field_name']];
            if ($flid = $flexinode->submit($subform, $form_state)) {
                $field_value = array('type' => 1, 'flid' => $flid);
                array_pop($element['#parents']);
                form_set_value($element, $field_value, $form_state);
            }
        } else {
            // nothing to save.
            unset($form_state['values'][$fieldname][$language][$delta]);
        }
    }


    // TODO subform validation!
}


function mfx_flexifield_widget_submit(&$form, &$form_state) {
    dpr($form_state['input']);
    dpr($form_state['values']);
    exit();
}


/**
 * create or returns the entity for the embedded form.
 */
function mfx_field_get_entity(&$item, $type = NULL) {
    if (isset($item['flid'])) {
        $item['entity'] = mfx_load_flexinode($item['flid']);
    }
    elseif (!isset($item['entity']) && isset($type)) {
        $item['entity'] = entity_get_controller('flexinode')->create(array('type' => $type));
    }
    return isset($item['entity']) ? $item['entity'] : FALSE;
}


