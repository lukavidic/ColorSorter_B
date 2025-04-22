import javafx.fxml.FXML;
import java.io.*;
import java.net.*;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import javafx.application.Platform;
import javafx.scene.control.RadioButton;
import javafx.scene.control.ToggleGroup;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.shape.Circle;

/**
 * @brief Controller class for managing the application GUI and communication with the microcontroller.
 * 
 * Handles network connections, user input, and real-time feedback visualization.
 */

public class Controller {

    /** @brief Button to start/stop the simulation process */
    @FXML private Button toggleButton;
    /** @brief Label for displaying operational status messages */
    @FXML private Label statusLabel;
    /** @brief Label showing current connection status to PIC */
    @FXML private Label connectionStatus;

	// Color disk radio buttons
    /** @brief Radio button for selecting left position of red color */
	@FXML private RadioButton redLeft;
    /** @brief Radio button for selecting right position of red color */
	@FXML private RadioButton redRight;

    /** @brief Radio button for selecting left position of green color */
    @FXML private RadioButton greenLeft;
    /** @brief Radio button for selecting right position of green color */
	@FXML private RadioButton greenRight;

    /** @brief Radio button for selecting left position of blue color */
    @FXML private RadioButton blueLeft;
    /** @brief Radio button for selecting right position of blue color */
	@FXML private RadioButton blueRight;

    /** @brief Radio button for selecting left position of yellow color */
    @FXML private RadioButton yellowLeft;
    /** @brief Radio button for selecting right position of yellow color */
    @FXML private RadioButton yellowRight;

    /** @brief Radio button for selecting left position of purple color */
    @FXML private RadioButton purpleLeft;
    /** @brief Radio button for selecting right position of purple color */
    @FXML private RadioButton purpleRight;

    /** @brief Radio button for selecting left position of orange color */
    @FXML private RadioButton orangeLeft;
    /** @brief Radio button for selecting right position of orange color */
	@FXML private RadioButton orangeRight;

    /** @brief Radio button for selecting left position of black color */
    @FXML private RadioButton blackLeft;
    /** @brief Radio button for selecting right position of black color */
	@FXML private RadioButton blackRight;

    /** @brief Radio button for selecting left position of white color */
    @FXML private RadioButton whiteLeft;
    /** @brief Radio button for selecting right position of white color */
	@FXML private RadioButton whiteRight;

    /** @brief Radio button for selecting left position of brown color */
    @FXML private RadioButton brownLeft;
    /** @brief Radio button for selecting right position of brown color */
    @FXML private RadioButton brownRight;

    /** @brief Visual indicator circle for left color position */
    @FXML private Circle leftCircle
    /** @brief Visual indicator circle for right color position */
    @FXML private Circle rightCircle;

    /** @brief Flag indicating if simulation is currently running */
    private volatile boolean isRunning = false;
    /** @brief Client socket for PIC connection */
    private Socket clientSocket;
    /** @brief Output stream to PIC */
    private PrintWriter out;
    /** @brief Input stream from PIC */
    private BufferedReader in;
    /** @brief Executor service for managing server thread */
    private ExecutorService executorService;

    /**
     * @brief Initializes controller components
     * 
     * Sets up radio button groups, default selections, and starts TCP server.
     * Automatically called by JavaFX after FXML loading.
     */
    @FXML
    public void initialize() {
		System.out.println("Test String ");
        updateConnectionState(false);
        updateStatus("Status: Waiting for connection...");
        startServer();
		
		//updateVisualFeedback("left","orange");
        // Set up ToggleGroups to prevent both options being selected for each color.
        ToggleGroup redGroup = new ToggleGroup();
        redLeft.setToggleGroup(redGroup);
        redRight.setToggleGroup(redGroup);
        
        ToggleGroup greenGroup = new ToggleGroup();
        greenLeft.setToggleGroup(greenGroup);
        greenRight.setToggleGroup(greenGroup);
        
        ToggleGroup blueGroup = new ToggleGroup();
        blueLeft.setToggleGroup(blueGroup);
        blueRight.setToggleGroup(blueGroup);
        
        ToggleGroup yellowGroup = new ToggleGroup();
        yellowLeft.setToggleGroup(yellowGroup);
        yellowRight.setToggleGroup(yellowGroup);
        
        ToggleGroup purpleGroup = new ToggleGroup();
        purpleLeft.setToggleGroup(purpleGroup);
        purpleRight.setToggleGroup(purpleGroup);
        
        ToggleGroup orangeGroup = new ToggleGroup();
        orangeLeft.setToggleGroup(orangeGroup);
        orangeRight.setToggleGroup(orangeGroup);
        
        ToggleGroup blackGroup = new ToggleGroup();
        blackLeft.setToggleGroup(blackGroup);
        blackRight.setToggleGroup(blackGroup);
        
        ToggleGroup whiteGroup = new ToggleGroup();
        whiteLeft.setToggleGroup(whiteGroup);
        whiteRight.setToggleGroup(whiteGroup);
        
		ToggleGroup brownGroup = new ToggleGroup();
        brownLeft.setToggleGroup(brownGroup);
        brownRight.setToggleGroup(brownGroup);

        // Setting default selections.
        redLeft.setSelected(true);
        greenLeft.setSelected(true);
        blueLeft.setSelected(true);
        yellowLeft.setSelected(true);
        purpleLeft.setSelected(true);
        orangeLeft.setSelected(true);
        blackLeft.setSelected(true);
        whiteLeft.setSelected(true);
		brownLeft.setSelected(true);
    }

    /**
     * @brief Cleans up network resources
     * 
     * Safely closes all open sockets and streams. Should be called whenever
     * the connection needs to be terminated.
     */
    private void cleanupConnection() {
        try {
            if (out != null) out.close();
            if (in != null) in.close();
            if (clientSocket != null && !clientSocket.isClosed()) {
                clientSocket.close();
            }
        } catch (IOException e) {
            System.err.println("Cleanup error: " + e.getMessage());
        }
    }

    /**
     * @brief Stops the server and cleans up resources
     * 
     * Shuts down executor service, closes connections, and updates UI
     * to reflect shutdown state.Is called on application exit.
     */
    public void stopServer() {

        cleanupConnection();
        if (executorService != null) {
            executorService.shutdownNow();
        }
		
		Platform.runLater(() -> {
        connectionStatus.setText("🔴 Shutdown");
        statusLabel.setText("Application closed");
    });
    }

    /**
     * @brief Starts TCP server in dedicated thread
     * 
     * Listens on port 8084 for incoming PIC connections. Manages
     * connection lifecycle and data reception.
     */
    private void startServer() {
	   executorService = Executors.newSingleThreadExecutor();
       executorService.execute(() -> {
           try (ServerSocket serverSocket = new ServerSocket(8084)) {
               updateUI("Waiting for PIC...");
               
               // Block until PIC connects (only 1 client needed)
               clientSocket = serverSocket.accept();
               out = new PrintWriter(clientSocket.getOutputStream(), true);
               in = new BufferedReader(new InputStreamReader(clientSocket.getInputStream()));
               
               updateUI("Connected to PIC!");
               updateConnectionState(true);  // Update to connected
               // Process incoming data in same thread
               String inputLine;
               while (true) {
                   inputLine = in.readLine();
                   if (inputLine == null) break; // PIC disconnected
                   processIncoming(inputLine);
               }
           } catch (IOException e) {
               updateUI("Error: " + e.getMessage());
           } finally {
               cleanupConnection();
           }
       });
   }
    
    /**
     * @brief Processes incoming messages from PIC
     * @param message The raw message received from microcontroller
     * 
     * Handles three types of messages:
     * 1. Movement commands (MOV#side#color)
     * 2. Start/Stop acknowledgments
     * 3. Generic data messages
     */
    private void processIncoming(String message) {
        // Filter out empty/AT messages
		if (message == null || message.isEmpty() || message.startsWith("AT")) return;

		System.out.println("Recieved: " + message);

		if(message.contains("MOV")) 
		{
			String[] arr = message.split("#");
			
			updateVisualFeedback(arr[1],arr[2]);
			return;
		}
		
        // Handle different message types
        if(message.equals("Start_ACK")) {
            updateUI("PIC confirmed Start");
        } else if(message.equals("Stop_ACK")) {
            updateUI("PIC confirmed Stop");
        } else {
            updateUI("Data: " + message);
        }
    }

    /**
     * @brief Handles start/stop button click
     * @param event JavaFX action event (automatic parameter)
     * 
     * Toggles simulation state and sends appropriate command to PIC.
     * Updates button text and connection state.
     */
    @FXML 
    private void handleToggle() {
        isRunning = !isRunning; // Change state
        String command = isRunning ? "Start\r\n" : "Stop\r\n";
        
        if (out != null && !clientSocket.isClosed()) {
            out.print(command);
			out.flush(); 
			//out.print(command);
            updateUI("Sent: " + command.trim());
            toggleButton.setText(isRunning ? "Stop" : "Start");
        } else {
            updateUI("No active connection!");
            isRunning = false; 
			updateConnectionState(false);
        }
    }

    /**
     * @brief Updates primary status label
     * @param message The status text to display
     */
    private void updateUI(String message) {
        javafx.application.Platform.runLater(() -> 
            statusLabel.setText(message)
        );
    }

    /**
     * @brief Handles configuration send button click
     * 
     * Gathers all color position selections, constructs configuration
     * message, and sends it to PIC. Validates connection state before sending.
     */
    @FXML
    private void handleSendConfig() {
        // Gather configuration selections for each color.
        // If no selection is made for a color, we use N - "None"
        String redPos = redLeft.isSelected() ? "L" : redRight.isSelected() ? "R" : "N";
        String greenPos = greenLeft.isSelected() ? "L" : greenRight.isSelected() ? "R" : "N";
        String bluePos = blueLeft.isSelected() ? "L" : blueRight.isSelected() ? "R" : "N";
        String yellowPos = yellowLeft.isSelected() ? "L" : yellowRight.isSelected() ? "R" : "N";
        String purplePos = purpleLeft.isSelected() ? "L" : purpleRight.isSelected() ? "R" : "N";
        String orangePos = orangeLeft.isSelected() ? "L" : orangeRight.isSelected() ? "R" : "N";
        String blackPos = blackLeft.isSelected() ? "L" : blackRight.isSelected() ? "R" : "N";
        String whitePos = whiteLeft.isSelected() ? "L" : whiteRight.isSelected() ? "R" : "N";
        String brownPos = brownLeft.isSelected() ? "L" : brownRight.isSelected() ? "R" : "N";
        // Build the configuration message.
        // We also append "\r\n" to clearly terminate the command.
        String configMessage = String.format("CFG:Red=%s,Green=%s,Blue=%s,Yellow=%s,Purple=%s,Orange=%s,Black=%s,White=%s,Brown=%s\r\n",
                redPos, greenPos, bluePos, yellowPos, purplePos, orangePos, blackPos, whitePos, brownPos);
        
        // Ensure that we have an active connection
        if (out != null && !clientSocket.isClosed()) {
            out.print(configMessage); // send the full configuration message
            out.flush();              // flush to ensure it is transmitted immediately
            updateStatus("Sent Config: " + configMessage.trim());
        } else {
            updateStatus("No active connection!");
        }
    }

    /**
     * @brief Updates status label with configuration information
     * @param message The configuration status text to display
     */
    private void updateStatus(String message) {
        Platform.runLater(() -> statusLabel.setText(message));
    }

    /**
     * @brief Updates connection status display
     * @param connected True if connection is active
     * 
     * Changes connection status label color and text,
     * enables/disables toggle button based on state.
     */
	private void updateConnectionState(boolean connected) {
		Platform.runLater(() -> {
			connectionStatus.setText(connected ? "🟢 Connected" : "🔴 Disconnected");
			connectionStatus.setStyle(connected ? "-fx-text-fill: green" : "-fx-text-fill: red");
			toggleButton.setDisable(!connected);
		});
	}

    /**
     * @brief Updates visual feedback circles
     * @param side Which circle to update ("left" or "right")
     * @param color CSS color name or hex value to set
     */
	public void updateVisualFeedback(String side, String color) {
        Platform.runLater(() -> {
            if ("left".equalsIgnoreCase(side)) {
                leftCircle.setStyle("-fx-fill: " + color + ";");
            } else if ("right".equalsIgnoreCase(side)) {
                rightCircle.setStyle("-fx-fill: " + color + ";");
            }
        });
    }
}