#include "MixerCV.hpp"

MixerCVModule::MixerCVModule() {
  config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
  for (int i = 0; i < MIXER_CHANNELS; i++) {
    channel_led_l[i] = 0.0f;
    channel_led_r[i] = 0.0f;
    mute[i] = false;
    solo[i] = false;

    mute_button[i] = new SynthDevKit::CV(0.5f);
    solo_button[i] = new SynthDevKit::CV(0.5f);
    channel_solo[i] = new SynthDevKit::CV(1.7f);
    channel_mute[i] = new SynthDevKit::CV(1.7f);

    configParam(MixerCVModule::VOLUME_SLIDER + i, 0.0f, 1.2f, 1.0f);
    configParam(MixerCVModule::PAN_PARAM + i, 0.0f, 1.0f, 0.5f);
    configParam(MixerCVModule::SOLO_PARAM + i, 0.0f, 1.0f, 0.0f);
    configParam(MixerCVModule::MUTE_PARAM + i, 0.0f, 1.0f, 0.0f);
    configParam(MixerCVModule::MIX_PARAM + i, 0.0f, 1.0f, 0.0f);
  }

  configParam(MixerCVModule::VOLUME_L_MAIN, 0.0f, 1.2f, 1.0f);
  configParam(MixerCVModule::VOLUME_R_MAIN, 0.0f, 1.2f, 1.0f);
  configParam(MixerCVModule::MUTE_L_PARAM, 0.0f, 1.0f, 0.0f);
  configParam(MixerCVModule::MUTE_R_PARAM, 0.0f, 1.0f, 0.0f);
  configParam(MixerCVModule::MAIN_L_MIX, 0.0f, 1.0f, 0.0f);
  configParam(MixerCVModule::MAIN_R_MIX, 0.0f, 1.0f, 0.0f);

  master_led_l = 0.0f;
  master_led_r = 0.0f;
  master_mute_l = false;
  master_mute_r = false;

  mute_l = new SynthDevKit::CV(1.7f);
  mute_r = new SynthDevKit::CV(1.7f);
  mute_l_param = new SynthDevKit::CV(0.5f);
  mute_r_param = new SynthDevKit::CV(0.5f);
}

void MixerCVModule::process(const ProcessArgs &args) {
  bool has_solo = false;
  float output_l[MIXER_CHANNELS];
  float output_r[MIXER_CHANNELS];
  float master_l = 0.0f;
  float master_r = 0.0f;

  master_led_l = 0.0f;
  master_led_r = 0.0f;

  // check for solo buttons first and handle the button presses and cv
  for (int i = 0; i < MIXER_CHANNELS; i++) {
    channel_solo[i]->update(inputs[SOLO_CV + i].getVoltage());
    channel_mute[i]->update(inputs[MUTE_CV + i].getVoltage());

    mute_button[i]->update(params[MUTE_PARAM + i].getValue());
    solo_button[i]->update(params[SOLO_PARAM + i].getValue());

    // buttons first
    if (solo_button[i]->newTrigger()) {
      solo[i] = !solo[i];
    }

    if (mute_button[i]->newTrigger()) {
      mute[i] = !mute[i];
    }

    // then cv
    if (channel_solo[i]->newTrigger()) {
      solo[i] = !solo[i];
    }

    if (channel_mute[i]->newTrigger()) {
      mute[i] = !mute[i];
    }

    // if any are solo, there's a solo
    if (solo[i]) {
      has_solo = true;
    }

    lights[SOLO_LIGHT + i].value = solo[i] ? 1.0f : 0.0f;
    lights[MUTE_LIGHT + i].value = mute[i] ? 1.0f : 0.0f;
  }

  // iterate through the channels
  for (int i = 0; i < MIXER_CHANNELS; i++) {
    output_l[i] = 0.0f;
    output_r[i] = 0.0f;

    // if the input is not active, or the input is muted
    if (mute[i] || (has_solo && !solo[i])) {
      // set everything to 0
      output_l[i] = 0.0f;
      output_r[i] = 0.0f;
    } else {
      // get the input
      float input = (inputs[INPUT + i].isConnected() ? inputs[INPUT + i].getVoltage() : 0.0f);

      // send the input if there's something listening
      if (outputs[SEND + i].isConnected()) {
        outputs[SEND + i].setVoltage(input);
      }

      // if there in recv, process it
      if (inputs[RECV + i].isConnected()) {
        // get the mix
        float mix = clamp((inputs[MIX_CV + i].isConnected() ? inputs[MIX_CV + i].getVoltage() / 10 : 0.0f) + params[MIX_PARAM + i].getValue(), 0.0f, 1.0f);

        // calculate the new "input"
        input = ((1 - mix) * input) + (mix * inputs[RECV + i].getVoltage());
      }

      // get the volume slider value
      float volume = params[VOLUME_SLIDER + i].getValue();

      // add any cv value
      volume = clamp((inputs[VOLUME_CV + i].isConnected() ? inputs[VOLUME_CV + i].getVoltage() / 10 : 0.0f) + volume, 0.0f, 1.2f);

      input *= volume;

      // figure out the left and right percentages
      float pan = params[PAN_PARAM + i].getValue();

      // add any cv value
      pan = clamp((inputs[PAN_CV + i].isConnected() ? inputs[PAN_CV + i].getVoltage() : 0) / 10 + pan, 0.0f, 1.0f);

      output_l[i] = output_r[i] = input;

      // determine the left/right mixes
      if (pan < 0.5f) {
        output_r[i] = (2.0f * pan) * output_r[i];
      }

      if (pan > 0.5f) {
        output_l[i] = (2.0f * (1.0f - pan)) * output_l[i];
      }

      // add the output to master
      // check if there is a solo, and it's not this one
      if (has_solo) {
        if (solo[i]) {
          master_l += output_l[i];
          master_r += output_r[i];
        }
      } else {
        master_l += output_l[i];
        master_r += output_r[i];
      }
    }

    // the led's
    channel_led_l[i] = fabsf(output_l[i]);
    channel_led_r[i] = fabsf(output_r[i]);
  }

  // check if muted
  mute_l_param->update(params[MUTE_L_PARAM].getValue());

  if (mute_l_param->newTrigger()) {
    master_mute_l = !master_mute_l;
  }

  if (inputs[MAIN_L_MUTE].isConnected()) {
    mute_l->update(inputs[MAIN_L_MUTE].getVoltage());

    if (mute_l->newTrigger()) {
      master_mute_l = !master_mute_l;
    }
  }

  if (master_mute_l) {
    master_l = 0.0f;
  } else {
    // if there's something to send to, send
    if (outputs[MAIN_L_SEND].isConnected()) {
      outputs[MAIN_L_SEND].setVoltage(master_l);
    }

    // if there is something to receive, calculate the new value
    if (inputs[MAIN_L_RECV].isConnected()) {
      // get the mix
      float mix = clamp((inputs[MAIN_MIX_L_CV].isConnected() ? inputs[MAIN_MIX_L_CV].getVoltage() : 0.0f) + params[MAIN_L_MIX].getValue(), 0.0f, 1.0f);

      // calculate the new "input"
      master_l = ((1 - mix) * master_l) + (mix * inputs[MAIN_L_RECV].getVoltage());
    }

    // apply the left volume
    float volume = params[VOLUME_L_MAIN].getValue();
    master_l = master_l * volume;
  }

  mute_r_param->update(params[MUTE_R_PARAM].getValue());

  if (mute_r_param->newTrigger()) {
    master_mute_r = !master_mute_r;
  }

  if (inputs[MAIN_R_MUTE].isConnected()) {
    mute_r->update(inputs[MAIN_R_MUTE].getVoltage());

    if (mute_r->newTrigger()) {
      master_mute_r = !master_mute_r;
    }
  }

  if (master_mute_r) {
    master_r = 0.0f;
  } else {
    // if there's something to send to, send
    if (outputs[MAIN_R_SEND].isConnected()) {
      outputs[MAIN_R_SEND].setVoltage(master_l);
    }

    // if there is something to receive, calculate the new value
    if (inputs[MAIN_R_RECV].isConnected()) {
      // get the mix
      float mix = clamp((inputs[MAIN_MIX_R_CV].isConnected() ? inputs[MAIN_MIX_R_CV].getVoltage() : 0.0f) + params[MAIN_R_MIX].getValue(), 0.0f, 1.0f);

      // calculate the new "input"
      master_r = ((1 - mix) * master_r) + (mix * inputs[MAIN_R_RECV].getVoltage());
    }

    // apply the left volume
    float volume = params[VOLUME_R_MAIN].getValue();
    master_r = master_r * volume;
  }


  lights[MUTE_L_MAIN].value = master_mute_l ? 1.0f : 0.0f;
  lights[MUTE_R_MAIN].value = master_mute_r ? 1.0f : 0.0f;

  // apply master volume to the led's
  master_led_l = fabsf(master_l);
  master_led_r = fabsf(master_r);

  // and to the output
  outputs[MAIN_L_OUT].setVoltage(master_l);
  outputs[MAIN_R_OUT].setVoltage(master_r);
}
