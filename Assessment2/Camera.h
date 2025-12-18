#pragma once

#include "Maths.h"
#include "Window.h"

class Camera {
public:
  Vec3 position;
  float yaw;
  float pitch;
  float speed;
  float sensitivity;
  bool firstFrame;

  // Jump
  float velocityY = 0.0f;
  bool isJumping = false;
  bool jumpedThisFrame = false;
  bool startedSprintingThisFrame = false;
  bool wasSprinting = false;
  const float gravity = 20.0f;
  const float jumpHeight = 1.5f;
  const float defaultGroundY = 1.8f;
  float currentGroundY = 1.8f; 

  Camera() {
    position = Vec3(0.0f, 1.8f, -5.0f);
    yaw = 0.0f;
    pitch = 0.0f;
    speed = 10.0f;
    sensitivity = 0.002f;
    firstFrame = true;
  }

  Matrix getViewMatrix() {
    Vec3 forward;
    forward.x = sinf(yaw) * cosf(pitch);
    forward.y = sinf(pitch);
    forward.z = cosf(yaw) * cosf(pitch);
    forward = forward.normalize();

    return Matrix::lookAt(position, position + forward, Vec3(0, 1, 0));
  }

  // Set current ground height based on collision detection
  void setGroundHeight(float groundY) { currentGroundY = groundY; }

  bool hasJumped() const { return jumpedThisFrame; }
  bool hasStartedSprinting() const { return startedSprintingThisFrame; }

  void update(Window *window, float dt, bool isSprinting = false) {
    int cx = window->width / 2;
    int cy = window->height / 2;

    if (firstFrame) {
      POINT pt = {cx, cy};
      ClientToScreen(window->hwnd, &pt);
      SetCursorPos(pt.x, pt.y);
      firstFrame = false;
      return;
    }

    int mx = window->getMouseInWindowX();
    int my = window->getMouseInWindowY();

    float dx = (float)(mx - cx);
    float dy = (float)(my - cy);

    yaw += dx * sensitivity;
    pitch -= dy * sensitivity;

    if (pitch > 1.5f)
      pitch = 1.5f;
    if (pitch < -1.5f)
      pitch = -1.5f;

    POINT pt = {cx, cy};
    ClientToScreen(window->hwnd, &pt);
    SetCursorPos(pt.x, pt.y);

    Vec3 forward;
    forward.x = sinf(yaw);
    forward.y = 0;
    forward.z = cosf(yaw);
    forward = forward.normalize();

    Vec3 right = Cross(Vec3(0, 1, 0), forward).normalize();

    float currentSpeed = isSprinting ? speed * 2.0f : speed;

    if (window->keys['W'])
      position += forward * currentSpeed * dt;
    if (window->keys['S'])
      position -= forward * speed * dt;
    if (window->keys['D'])
      position += right * speed * dt;
    if (window->keys['A'])
      position -= right * speed * dt;

    // Only jump if on ground
    jumpedThisFrame = false;
    if (window->keys[VK_SPACE] && !isJumping && position.y <= currentGroundY + 0.1f) {
      velocityY = sqrtf(2.0f * gravity * jumpHeight);
      isJumping = true;
      jumpedThisFrame = true;
    }

    startedSprintingThisFrame = false;
    if (isSprinting && !wasSprinting) {
      startedSprintingThisFrame = true;
    }
    wasSprinting = isSprinting;

    velocityY -= gravity * dt;
    position.y += velocityY * dt;

    if (position.y <= currentGroundY) {
      position.y = currentGroundY;
      velocityY = 0.0f;
      isJumping = false;
    }

    if (position.y < -10.0f) {
      position.y = defaultGroundY;
      velocityY = 0.0f;
      isJumping = false;
    }
  }
};