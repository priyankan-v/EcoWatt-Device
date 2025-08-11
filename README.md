# EcoWatt-Device

## Wokwi Project
https://wokwi.com/projects/438990299357436929

### Install the Draw.io VS Code Extension
### Create a new file with extension .drawio, .drawio.png (e.g., ecoWattDiagram.drawio.png)


## **What Milestone 1 Really Is**

It’s two things:

1. **Model your device’s basic behavior in Petri Nets** — *this is like drawing the system’s “flowchart on steroids” that shows states, actions, and how data moves.*
2. **Build a small working prototype (“scaffold”)** — *this is not the full EcoWatt system, just a baby version that simulates the core loop.*

---

## **Step 1 – Understand the Target Workflow**

You’re **only** modeling and prototyping one key loop:

1. **Periodic Polling** → Read inverter values (voltage/current).
2. **Buffering** → Store them locally in memory.
3. **Periodic Upload** → Every “15 minutes” (but we simulate with 15 seconds), send the collected data to the cloud.

The rest of the fancy stuff (FOTA, security, power management) **comes later**, not in Milestone 1.

---

## **Step 2 – Petri Net Modeling**

You need to create a Petri Net that represents the above loop.

### **What is a Petri Net?**

It’s made of:

* **Places** (circles) → States or resources. E.g., *“Waiting to poll”, “Buffer has data”*.
* **Transitions** (rectangles/bars) → Actions or events. E.g., *“Read from inverter”, “Upload data”*.
* **Tokens** (dots) → Something that “flows” — here, they can represent *time ticks* or *data availability*.

Example structure (simplified):

```
[Start] → (Poll inverter) → [Data in buffer] → (Upload trigger) → [Buffer empty]
```

Where:

* **Places**: Start, Data in buffer, Buffer empty.
* **Transitions**: Poll inverter, Upload trigger.

You just have to show:

* That polling happens repeatedly.
* Data accumulates in a buffer.
* Every 15 seconds, an upload clears the buffer.

**Tip:** You can use **draw\.io** or **Yasper** to make this — avoid overcomplicating, you don’t need all device details yet.

---

## **Step 3 – Scaffold Implementation**

Think of this like a *dummy* EcoWatt device.

### **What it must do:**

1. **Poll data**

   * Either talk to the Inverter SIM **or** just generate fake readings:

     ```python
     voltage = random.randint(220, 240)
     current = random.uniform(0.5, 2.0)
     ```
   * Do this every few seconds.

2. **Buffer data**

   * Store readings in a list:

     ```python
     buffer.append((timestamp, voltage, current))
     ```

3. **Upload periodically (every 15s)**

   * Print the buffer contents (simulating a send to cloud).
   * Clear the buffer.

4. **Keep it modular**:

   * `acquire_data()` → Get a reading.
   * `store_data()` → Add to buffer.
   * `upload_data()` → Print & clear buffer.
   * `main_loop()` → Handles timing and calling the above.

---

## **Step 4 – Suggested Plan of Attack**

I’d do it in this order:

1. **Make Petri Net** → This forces you to fully understand the loop before coding.
2. **Write a very basic scaffold in Python** (faster for testing logic).
3. **Test timing** → Make sure polling and uploading work on schedule.
4. **Record a short screen capture** showing:

   * Petri Net diagram explanation.
   * Scaffold code running.

---

## **Key Things to Remember**

* Milestone 1 is about **concept + proof of concept**, not performance or final hardware.
* You **don’t** need ESP8266 or real inverter for this yet — simulation is allowed.
* You’re **laying the foundation** — this will guide you for Milestone 2.

---

If you want, I can **draw your Petri Net** and **write the scaffold Python code** so you have a working starting point before you move to ESP8266. That way, you’ll hit both parts of Milestone 1 quickly.

Do you want me to do that next?
